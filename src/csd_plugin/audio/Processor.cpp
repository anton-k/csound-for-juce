#include "csd_plugin/audio/MidiBuffer.h"
#include <algorithm>
#include <cmath>
#include <csd_plugin/audio/Logger.h>
#include <csd_plugin/audio/Processor.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <csound/sysdep.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <memory>
#include <vector>

namespace csd_plugin {

namespace {

inline MYFLT wrap_limiter(MYFLT sample) {
  if (!std::isfinite(static_cast<double>(sample))) {
    return MYFLT{0};
  }

  return std::clamp(sample, -WRAP_VOLUME_LIMIT, WRAP_VOLUME_LIMIT);
}

bool is_control_channel_type(controlChannelInfo_t info) {
  return (info.type & CSOUND_CHANNEL_TYPE_MASK) == CSOUND_CONTROL_CHANNEL;
}

constexpr unsigned int MAX_COMPILATION_LOG_CHARS = 65536;

} // namespace

CsoundSettings::CsoundSettings() : ksmps(1), out_size(2), in_size(0) {}

void CsoundSettings::prepare(Csound *csound) {
  ksmps = static_cast<int>(csound->GetKsmps());

  // Do not query audio channel counts here.
  //
  // The channel-count query API is unreliable across Csound versions and host
  // states, especially during sample-rate switches and repeated prepareToPlay()
  // calls. The plugin's declared IOLayout is authoritative for channel counts.

  sample_rate = static_cast<int>(csound->GetSr());
  zero_dbfs = static_cast<MYFLT>(csound->Get0dBFS());

  if (zero_dbfs < static_cast<MYFLT>(0.01)) {
    zero_dbfs = static_cast<MYFLT>(1.f);
    inverse_zero_dbfs = static_cast<MYFLT>(1.f);
  } else {
    inverse_zero_dbfs = static_cast<MYFLT>(1.f) / zero_dbfs;
  }

  set_channel_names(csound);
}

void CsoundSettings::set_channel_names(Csound *csound) {
  controlChannelInfo_t *channel_list = nullptr;
  channel_names.clear();

  int num_channels = csound->ListChannels(channel_list);

  if (num_channels > 0 && channel_list != nullptr) {
    for (int32_t i = 0; i < num_channels; ++i) {
      if (is_control_channel_type(channel_list[i])) {
        channel_names.push_back(channel_list[i].name);
      }
    }
  }

  csound->DeleteChannelList(channel_list);
}

bool CsdOutputAudioBuffer::read(MYFLT &sample) {
  if (read_index >= capacity) {
    sample = default_value;
    return false;
  } else {
    if (ptr == nullptr) {
      sample = default_value;
      return false;
    }
    sample = wrap_limiter(ptr[read_index] * scale);
    read_index++;
    return true;
  }
}

int Processor::get_latency_samples() {
  int csound_processing_latency =
      (io_layout.get_total_in_size() > 0) ? csound_settings.ksmps : 0;

  return io_layout.extra_latency_samples + csound_processing_latency;
}

void Processor::write_input(MYFLT sample) {
  // CsdInputAudioBuffer already applies zero_dbfs scaling.
  audio_buffers.write(sample);
}

bool Processor::read_output(MYFLT &sample) {
  // CsdOutputAudioBuffer already applies inverse_zero_dbfs scaling.
  bool read_success = audio_buffers.read(sample);

  if (!read_success) {
    sample = 0.0; // Prevent stale memory/feedback loops on underflow
  }

  return read_success;
}

void Processor::set_host_io() {
#if defined(CS_VERSION) && CS_VERSION >= 7
  csound->SetHostAudioIO();
  csound->SetHostMIDIIO();
#else
  const int play = (io_layout.get_out_size() > 0) ? 1 : 0;

  const int rec = (io_layout.get_total_in_size() > 0) ? 1 : 0;

  csoundSetHostImplementedAudioIO(csound->GetCsound(), play, rec);

  const int host_midi_io =
      (io_layout.has_midi_in || io_layout.has_midi_out) ? 1 : 0;

  csoundSetHostImplementedMIDIIO(csound->GetCsound(), host_midi_io);
#endif
}

void Processor::set_csound_midi_callbacks() {
  csound->SetExternalMidiInOpenCallback(&Processor::midi_device_open);
  csound->SetExternalMidiInCloseCallback(&Processor::midi_device_close);
  csound->SetExternalMidiReadCallback(&Processor::midi_read);

  csound->SetExternalMidiOutOpenCallback(&Processor::midi_device_open);
  csound->SetExternalMidiOutCloseCallback(&Processor::midi_device_close);
  csound->SetExternalMidiWriteCallback(&Processor::midi_write);
}

int Processor::midi_device_open(CSOUND *csound_, void **user_data,
                                const char *devName) {
  auto csound_host_data = csoundGetHostData(csound_);
  *user_data = (void *)csound_host_data;
  return 0;
}

int Processor::midi_device_close(CSOUND *csound_, void *user_data) { return 0; }

int Processor::midi_read(CSOUND *csound, void *userData, unsigned char *buf,
                         int max_size) {
  void *host_data =
      (userData != nullptr) ? userData : csoundGetHostData(csound);
  auto *proc = static_cast<Processor *>(host_data);

  if (!proc || !(proc->get_io_layout().has_midi_in) || max_size <= 0 ||
      buf == nullptr)
    return 0;

  auto &queue = proc->midi_buffers.in();

  const int64_t cycle_end_sample = proc->get_cycle_end_sample();

  int bytes_written = 0;
  RawMidiEvent next_event;

  while (queue.peek(next_event)) {
    const int msg_size = next_event.size;

    if (msg_size == 0) {
      queue.pop();
      continue;
    }

    // Avoid getting stuck forever on an event that cannot fit into Csound's
    // buffer.
    if (msg_size > max_size) {
      queue.pop();
      continue;
    }

    if (next_event.samplePosition < cycle_end_sample) {
      if (bytes_written + msg_size > max_size) {
        break;
      }

      queue.pop();

      std::memcpy(buf + bytes_written, next_event.data, msg_size);
      bytes_written += msg_size;
    } else {
      // Event belongs to a future Csound cycle.
      break;
    }
  }

  return bytes_written;
}

int Processor::midi_write(CSOUND *csound_, void *userData,
                          const unsigned char *midi_buffer,
                          int midi_buffer_size) {
  void *host_data =
      (userData != nullptr) ? userData : csoundGetHostData(csound_);
  Processor *processor = static_cast<Processor *>(host_data);

  if (!processor || !(processor->get_io_layout().has_midi_out))
    return 0;

  if (midi_buffer_size <= 0 || midi_buffer == nullptr)
    return 0;

  const int safe_int_size = std::min(midi_buffer_size, MIDI_DATA_SIZE);
  const uint8_t safe_size = static_cast<uint8_t>(safe_int_size);

  csd_plugin::RawMidiEvent midi_event{processor->get_cycle_end_sample(),
                                      midi_buffer, safe_size};

  processor->midi_buffers.out().push(midi_event);

  return 0;
}

void Processor::stop_and_reset_csound() {
  if (csound == nullptr) {
    return;
  }

  audio_buffers = CsdAudioBuffers();

  // Detach host data first so Csound callbacks cannot find this Processor
  // while we are shutting the engine down.
  csound->SetHostData(nullptr);

  // Destroy the Csound instance completely. The unique_ptr destructor
  // handles all internal Csound cleanup safely. This is the most reliable
  // way to handle sample rate changes, as csound->Reset() does not clear
  // compiled instruments or options.
  csound.reset();
}

bool Processor::setup_csound(int sample_rate) {
  const int safe_sample_rate = (sample_rate > 0) ? sample_rate : 44100;

  // Always create a fresh Csound instance
  csound = std::make_unique<Csound>();

  csound->SetHostData(this);
  set_host_io();
  csound->SetMessageCallback(csound_message_callback);

  set_csound_midi_callbacks();

  // IMPORTANT: Csound's SetOption may store the pointer and read it later
  // during CompileCSD. We must ensure the strings stay alive until then.
  // Passing .c_str() of a temporary std::format result is a use-after-free!
  std::string opt_r = std::format("-r{}", safe_sample_rate);

  csound->SetOption("-d");
  csound->SetOption("-m0");
  csound->SetOption("-+rtmidi=NULL");
  csound->SetOption("-M0");
  csound->SetOption(opt_r.c_str());
  csound->SetOption("-Q0");

  if (io_layout.get_total_in_size() > 0) {
    csound->SetOption("-i adc");
  }

  if (io_layout.get_out_size() > 0) {
    csound->SetOption("-o dac");
  } else {
    csound->SetOption("-n");
  }

  logger.is_compiling = true;
  logger.compilation_log_buffer.clear();

  int compile_result = csound->CompileCSD(csd_file_content.c_str(), 1);
  int start_result = 0;

  if (compile_result == 0) {
    // Do not re-assert host I/O here.
    //
    // Some Csound versions expect host I/O configuration to remain stable
    // around compilation/start. Re-asserting it after CompileCSD() can
    // disturb the audio I/O state and cause the plugin to go silent.
    start_result = csound->Start();
  }

  logger.is_compiling = false;

  if (compile_result != 0 || start_result != 0) {
    // FAILURE: Copy the main-thread string into the RT-safe buffer
    const char *error_text = logger.compilation_log_buffer.empty()
                                 ? "Csound compilation or start failed"
                                 : logger.compilation_log_buffer.c_str();

    logger.set_last_error(error_text);
    log(csd_plugin::LogLevel::Error, error_text);

    ready_to_play.store(false, std::memory_order_release);
    std::string().swap(logger.compilation_log_buffer);
    return false;
  }

  // SUCCESS: Clear any previous errors
  logger.clear_last_error();
  std::string().swap(logger.compilation_log_buffer);
  return true;
}

bool Processor::prepare_to_play(int host_sample_rate) {
  ready_to_play.store(false, std::memory_order_release);
  logger.clear_last_error();
  timer.reset();

  const int sample_rate = (host_sample_rate > 0) ? host_sample_rate : 44100;

  if (!prepare_csound_to_play(sample_rate)) {
    stop_and_reset_csound();
    log(LogLevel::Error, "prepare_to_play: prepare_csound_to_play failed");
    return false;
  }

  if (!validate_io_layout()) {
    stop_and_reset_csound();
    log(LogLevel::Error,
        "prepare_to_play: csound channels are inconsistent with io_layout");
    return false;
  }

  midi_buffers.clear();
  prepare_audio_buffers();

  ready_to_play.store(true, std::memory_order_release);
  log(LogLevel::Info, "prepare_to_play: success, ready_to_play = true");
  log(LogLevel::Info,
      std::format("\ncsd_in {}, csd_out {}", csound->GetChannels(1),
                  csound->GetChannels(0))
          .c_str());
  log(LogLevel::Info,
      std::format("\nin {}, out {}", io_layout.get_total_in_size(),
                  io_layout.get_out_size())
          .c_str());

  return true;
}

bool Processor::prepare_csound_to_play(int sample_rate) {
  const bool needs_csound_reinit =
      (csound == nullptr) || (csound_settings.sample_rate != sample_rate);

  if (needs_csound_reinit) {
    stop_and_reset_csound();
    if (!setup_csound(sample_rate)) {
      return false;
    }

    csound_settings.prepare(csound.get());
  } else {
    if (!csound) {
      return false;
    }
  }

  csound_settings.out_size = io_layout.get_out_size();
  csound_settings.in_size = io_layout.get_total_in_size();
  return true;
}

void Processor::prepare_audio_buffers() {
  audio_buffers = CsdAudioBuffers(csound.get(), csound_settings, io_layout);
}

bool Processor::process() {
  if (!ready_to_play.load())
    return false;

  bool ok = csound->PerformKsmps() == 0;

  if (!ok) {
    for (int retry_index = 0; retry_index < 2; ++retry_index) {
      csound->Reset();
      ok = csound->PerformKsmps() == 0;
      if (ok) {
        break;
      }
    }
  }

  // Reset input state for the next Csound cycle.
  audio_buffers.reset();

  // Expose output only if Csound actually produced a valid cycle.
  //
  // If PerformKsmps() failed, Csound's spout may contain stale or invalid
  // data. Keeping the output buffer empty makes the caller produce silence
  // instead of harsh noise.
  if (ok) {
    audio_buffers.csound_performed();
  }

  timer.next(csound_settings.ksmps);
  return ok;
}

void Processor::release_resources() {
  ready_to_play.store(false, std::memory_order_release);
  logger.log_callback = nullptr;
  stop_and_reset_csound();
  clear_buffers();
  timer.reset();
}

void Processor::shutdown() {
  ready_to_play.store(false, std::memory_order_release);
  logger.log_callback = nullptr;
  stop_and_reset_csound();
  clear_buffers();
  timer.reset();
}

static int ceil_div(int a, int b) { return (a + b - 1) / b; }

int Processor::get_csound_cycle_size(int block_size) {
  const int ksmps = csound_settings.ksmps;

  if (block_size <= 0 || ksmps <= 0)
    return 0;

  const int missing_frames =
      block_size - audio_buffers.available_output_frames();

  if (missing_frames <= 0)
    return 0;

  return ceil_div(missing_frames, ksmps);
}

void Processor::clear_buffers() {
  audio_buffers.clear();
  midi_buffers.clear();
}

void Processor::csound_message_callback(CSOUND *csound, int attr,
                                        const char *format, va_list val) {
  char buffer[2048];
  vsnprintf(buffer, sizeof(buffer), format, val);

  int type = attr & 0x7;
  csd_plugin::LogLevel level = csd_plugin::LogLevel::Info;

  if (type == 1) {
    level = csd_plugin::LogLevel::Error;
  } else if (type == 2) {
    level = csd_plugin::LogLevel::Warning;
  }

  auto *processor =
      static_cast<csd_plugin::Processor *>(csoundGetHostData(csound));

  if (!processor) {
    return;
  }

  if (processor->logger.is_compiling) {
    // MAIN THREAD ONLY: Safe to use std::string concatenation
    std::string msg(buffer);

    if (!msg.empty() && msg != "\n") {
      if (processor->logger.compilation_log_buffer.size() <
          MAX_COMPILATION_LOG_CHARS) {
        processor->logger.compilation_log_buffer += msg;

        if (msg.back() != '\n') {
          processor->logger.compilation_log_buffer += '\n';
        }

        if (processor->logger.compilation_log_buffer.size() >
            MAX_COMPILATION_LOG_CHARS) {
          processor->logger.compilation_log_buffer.resize(
              MAX_COMPILATION_LOG_CHARS);
        }
      }
    }
  } else {
    processor->log(level, buffer);
  }
}

bool Processor::validate_io_layout() {
  if (csound.get() == nullptr) {
    return false;
  }

  const int csound_in_channels = csound.get()->GetChannels(1);
  const int csound_out_channels = csound.get()->GetChannels(0);

  const int expected_in = io_layout.get_total_in_size();
  const int expected_out = io_layout.get_out_size();

  if (csound_in_channels != expected_in) {
    // Csound bug: csound reports nchnls_i = 0 as 1 with API call.
    if (expected_in == 0 && csound_in_channels == 1) {
      return true;
    }
    return false;
  }

  if (csound_out_channels != expected_out) {
    return false;
  }

  return true;
}

} // namespace csd_plugin
