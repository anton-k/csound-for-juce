
#include "csd_plugin/audio/AudioBuffer.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include <csound/sysdep.h>
#include <cstdint>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <format>
#include <csd_plugin/audio/Logger.h>
#include <csd_plugin/audio/Processor.h>
#include <thread>
#include <cstring>


namespace csd_plugin {

namespace {

inline MYFLT wrap_limiter(MYFLT sample)
{
    if (!std::isfinite(static_cast<double>(sample)))
    {
        return MYFLT{0};
    }

    return std::clamp(
        sample,
        -WRAP_VOLUME_LIMIT,
        WRAP_VOLUME_LIMIT
    );
}



bool is_control_channel_type(controlChannelInfo_t info) {
  return (info.type & CSOUND_CHANNEL_TYPE_MASK) == CSOUND_CONTROL_CHANNEL;
}

int get_desired_audio_buffer_size(int channels, int max_block_size, int ksmps)
{
    if (channels <= 0)
        return 0;

    // Some hosts can behave oddly. Keep a conservative hard floor.
    const int safe_block_size = std::max(max_block_size * 2, 8192);

    // Worst-case output occupancy should stay below block_size + ksmps.
    // Use extra safety margin.
    const int safety_frames = std::max(4 * ksmps, 256);

    const int frames = safe_block_size + safety_frames;

    return frames * channels;
}


}


CsoundSettings::CsoundSettings(): ksmps(1), out_size(2), in_size(0) {}

void CsoundSettings::prepare(Csound* csound) {
    ksmps = static_cast<size_t>(csound->GetKsmps());
    out_size = static_cast<size_t>(csound->GetChannels(0));
    in_size = static_cast<size_t>(csound->GetChannels(1));
    sample_rate = csound->GetSr();
    zero_dbfs = static_cast<MYFLT>(csound->Get0dBFS());
    if (zero_dbfs < static_cast<MYFLT>(0.01)) {
        zero_dbfs = static_cast<MYFLT>(1.f);
        inverse_zero_dbfs = static_cast<MYFLT>(1.f);
    } else {
        inverse_zero_dbfs = static_cast<MYFLT>(1.f) / zero_dbfs;
    }
    set_channel_names(csound);
}

void CsoundSettings::set_channel_names(Csound* csound) {
  controlChannelInfo_t* channel_list = nullptr;
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

int Processor::get_latency_samples() {
    int csound_processing_latency =
        (io_layout.get_total_in_size() > 0)
            ? csound_settings.ksmps
            : 0;
    return io_layout.extra_latency_samples + csound_processing_latency;
}

void Processor::write_input(MYFLT sample) {
    audio_buffers.in().write(csound_settings.zero_dbfs * sample);
}

bool Processor::read_output(MYFLT& sample) {
    bool read_success = audio_buffers.out().read(sample);

    if (!read_success) {
        sample = 0.0; // Prevent stale memory/feedback loops on underflow
    } else {
        sample = wrap_limiter(csound_settings.inverse_zero_dbfs * sample);
    }

    return read_success;
}

void Processor::set_host_io() {
    #if defined(CS_VERSION) && CS_VERSION >= 7
        csound->SetHostAudioIO();
        csound->SetHostMIDIIO();
    #else
        // Fallback for Csound 6 using the underlying C API directly via GetCsound()
        csoundSetHostImplementedAudioIO(csound->GetCsound(), 1, 0);
        csound->SetHostImplementedMIDIIO(1);
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

int Processor::midi_device_open(CSOUND *csound_, void **user_data, const char *devName) {
    auto csound_host_data = csoundGetHostData(csound_);
    *user_data = (void *)csound_host_data;
    return 0;
}

int Processor::midi_device_close(CSOUND *csound_, void *user_data)
{
    return 0;
}

int Processor::midi_read(CSOUND* csound, void* userData, unsigned char* buf, int max_size) {
    auto* proc = static_cast<Processor*>(userData);
    if (!proc || !(proc->get_io_layout().has_midi_in)) return 0;

    auto& queue = proc->midi_buffers.in();
    int cycle_end_sample = proc->current_cycle_end_sample;

    int bytes_written = 0;
    RawMidiEvent next_event;

    while (queue.peek(next_event)) {
        int msg_size = next_event.size;

        if (next_event.samplePosition < cycle_end_sample) {
            if (bytes_written + msg_size > max_size) {
                break;
            }

            queue.pop();
            std::memcpy(buf + bytes_written, next_event.data, msg_size);
            bytes_written += msg_size;
        } else {
            break; // Event is for the future cycle
        }
    }

    return bytes_written;
}

int Processor::midi_write(CSOUND *csound_, void *userData, const unsigned char *midi_buffer, int midi_buffer_size)
{
    auto csound_host_data = csoundGetHostData(csound_);
    Processor *processor = static_cast<Processor *>(csound_host_data);
    if (!processor || !(processor->get_io_layout().has_midi_out)) return 0;

    const uint8_t safeSize = static_cast<uint8_t>(std::min(midi_buffer_size, MIDI_DATA_SIZE));

    csd_plugin::RawMidiEvent midi_event{
        processor->get_current_sample(),
        midi_buffer,
        safeSize
    };

    processor->midi_buffers.out().push(midi_event);
    return 0;
}

bool Processor::setup_csound(int sample_rate) {
    csound = std::unique_ptr<Csound>(new Csound());

    csound->SetHostData(this);
    set_host_io();
    csound->SetMessageCallback(csound_message_callback);

    set_csound_midi_callbacks();
    // TODO consider making m0 configurable and allow m127 for debug
    std::string options = std::format("-n -d -b0 -+rtmidi=NULL -M0 -sr {} -Q0 -m0", static_cast<int>(sample_rate));
    csound->SetOption(options.c_str());

    is_compiling = true;
    compilation_log_buffer.clear();

    int compile_result = csound->CompileCSD(csd_file_content.c_str(), 1);
    int start_result = 0;
    if (compile_result == 0) {
        start_result = csound->Start();
    }

    is_compiling = false;

    if (compile_result != 0 || start_result != 0) {
        // FAILURE: Copy the main-thread string into the RT-safe buffer
        set_last_error(compilation_log_buffer.c_str());

        log(csd_plugin::LogLevel::Error, compilation_log_buffer.c_str());
        ready_to_play = false;
        return false;
    }

    // SUCCESS: Clear any previous errors
    clear_last_error();
    return true;
}

void Processor::prepare_to_play(int sample_rate, int max_block_size)
{
    ready_to_play = prepare_csound_to_play(sample_rate);

    if (!ready_to_play) {
        return;
    }
/*
    if (!validate_io_layout()) {
        ready_to_play = false;
        return;
    }
*/
    prepare_audio_buffers(max_block_size);

    current_sample = 0;
    current_cycle_end_sample = 0;
    midi_buffers.clear();
}

bool Processor::prepare_csound_to_play(int sample_rate) {
    //
    // 1. Csound re-initialization condition:
    // Re-init ONLY if not ready (uninitialized/failed) OR sample_rate changed.
    bool was_ready = ready_to_play.load();
    bool needs_csound_reinit = !was_ready || (csound_settings.sample_rate != sample_rate);

    // Block audio thread
    ready_to_play = false;
    while (is_processing_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }


    // Re-initialize Csound if needed
    if (needs_csound_reinit) {
        if (csound != nullptr) {
            csound->SetHostData(nullptr);
            log_callback = nullptr;
            csound.reset();
        }

        if (setup_csound(sample_rate)) {
            csound_settings.prepare(csound.get());
        } else {
            return false;
        }
    }
    return true;
}

void Processor::prepare_audio_buffers(int max_block_size)
{
    const int in_size = io_layout.get_total_in_size();
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    const int desired_in_capacity =
        get_desired_audio_buffer_size(in_size, max_block_size, ksmps);

    const int desired_out_capacity =
        get_desired_audio_buffer_size(out_size, max_block_size, ksmps);

    audio_buffers.reset(desired_in_capacity, desired_out_capacity);

    current_max_block_size = std::max(max_block_size, 8192);

    resync_audio_buffers();
}



void Processor::process_block(int block_size)
{
    if (!ready_to_play || block_size <= 0)
        return;

    is_processing_.store(true, std::memory_order_release);

    csound_process(block_size);
    ensure_output_for_block(block_size);

    is_processing_.store(false, std::memory_order_release);
}


void Processor::release_resources() {
    ready_to_play = false;

    if (csound == nullptr) {
        return;
    }

    while (is_processing_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    csound->SetHostData(nullptr);
    krate_callback = nullptr;
    log_callback = nullptr;
    csound.reset();

    clear_buffers();
    current_sample = 0;
    current_max_block_size = 0;
}

static int ceil_div(int a, int b)
{
    return (a + b - 1) / b;
}

int Processor::get_csound_cycle_size(int block_size)
{
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    if (block_size <= 0 || out_size <= 0 || ksmps <= 0)
        return 0;

    const int available_out_frames =
        audio_buffers.out().get_size() / out_size;

    const int missing_frames = block_size - available_out_frames;

    if (missing_frames <= 0)
        return 0;

    return ceil_div(missing_frames, ksmps);
}


void Processor::csound_process(int block_size)
{
    const int in_size = io_layout.get_total_in_size();
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    if (block_size <= 0 || out_size <= 0 || ksmps <= 0)
        return;

    const int in_cycle_samples = ksmps * in_size;
    const int out_cycle_samples = ksmps * out_size;

    csound_cycle_size = get_csound_cycle_size(block_size);

    for (int cycle_index = 0; cycle_index < csound_cycle_size; ++cycle_index)
    {
        if (audio_buffers.out().get_free_space() < out_cycle_samples)
        {
            set_last_error("Csound output FIFO overflow risk; resyncing");
            log(LogLevel::Error, "Csound output FIFO overflow risk; resyncing");
            resync_audio_buffers();
            break;
        }

        current_cycle_end_sample = current_sample + ksmps;

        if (in_size > 0)
        {
            MYFLT* spin = csound->GetSpin();

            const int was_read =
                audio_buffers.in().read_block_partial(spin, in_cycle_samples);

            std::fill(
                spin + was_read,
                spin + in_cycle_samples,
                MYFLT{0}
            );
        }

        if (krate_callback)
        {
            krate_callback();
        }

        csound->PerformKsmps();

        const MYFLT* spout = csound->GetSpout();

        const bool write_success =
            audio_buffers.out().write_block(spout, out_cycle_samples);

        if (!write_success)
        {
            set_last_error("Csound output FIFO write failed; resyncing");
            resync_audio_buffers();
            break;
        }

        current_sample = current_cycle_end_sample;
    }
}



void Processor::clear_buffers() {
    audio_buffers.clear();
    midi_buffers.clear();
}

int64_t Processor::get_current_sample() {
    return current_sample;
}

void Processor::csound_message_callback(CSOUND* csound, int attr, const char* format, va_list val)
{
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, val);

    int type = attr & 0x7;
    csd_plugin::LogLevel level = csd_plugin::LogLevel::Info;
    if (type == 1) level = csd_plugin::LogLevel::Error;
    else if (type == 2) level = csd_plugin::LogLevel::Warning;

    auto* processor = static_cast<csd_plugin::Processor*>(csoundGetHostData(csound));
    if (!processor) return;

    if (processor->is_compiling) {
        // MAIN THREAD ONLY: Safe to use std::string concatenation
        std::string msg(buffer);
        if (!msg.empty() && msg != "\n") {
            processor->compilation_log_buffer += msg;
            if (msg.back() != '\n') {
                processor->compilation_log_buffer += '\n';
            }
        }
    } else {
        // AUDIO THREAD: 100% RT-safe logging
        processor->log(level, buffer);

        // If a runtime error occurs, capture it without allocation
        if (level == csd_plugin::LogLevel::Error) {
            processor->set_last_error(buffer);
        }
    }
}

bool Processor::validate_io_layout()
{
    if (!csound) {
        set_last_error("Csound instance is null");
        return false;
    }

    if (csound_settings.out_size != io_layout.get_out_size()) {
        set_last_error("IOLayout output channels do not match Csound nchnls");
        return false;
    }

    if (csound_settings.in_size != io_layout.get_total_in_size()) {
        set_last_error("IOLayout input channels do not match Csound nchnls_i");
        return false;
    }

    return true;
}

void Processor::resync_audio_buffers()
{
    audio_buffers.clear();
    midi_buffers.clear();

    const int in_size = io_layout.get_total_in_size();
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    // For FX plugins, prefill one Csound cycle of silence.
    // This creates the expected initial latency and prevents first-block
    // input underflow.
    if (in_size > 0 && out_size > 0 && ksmps > 0) {
        const int prefill_samples = ksmps * out_size;

        for (int i = 0; i < prefill_samples; ++i) {
            audio_buffers.out().write(0.0);
        }
    }
}

void Processor::ensure_output_for_block(int block_size)
{
    const int out_size = io_layout.get_out_size();

    if (out_size <= 0 || block_size <= 0)
        return;

    const int needed_samples = block_size * out_size;
    const int have_samples = audio_buffers.out().get_size();

    if (have_samples >= needed_samples)
        return;

    const int missing_samples = needed_samples - have_samples;

    int written = 0;

    for (int i = 0; i < missing_samples; ++i) {
        if (audio_buffers.out().write(0.0)) {
            ++written;
        }
    }

    if (written != missing_samples) {
        set_last_error("Csound output FIFO underflow and padding failed");
    } else {
        set_last_error("Csound output FIFO underflow; padded with silence");
    }
}


}
