#include "csd_plugin/audio/Processor.h"
#include "csd_plugin/audio/Logger.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <cstdint>
#include <cstring>
#include <format>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_csd/audio/CsoundLogConsumer.h>
#include <juce_csd/audio/Processor.h>

namespace juce_csd {

Processor::Processor(const std::string &csd_file_content,
                     const csd_plugin::IOLayout &io_layout,
                     const ParameterSpec &parameter_spec,
                     juce::AudioProcessor &processor)
    : csound(csd_file_content, io_layout),
      parameters(processor, parameter_spec), log_queue(log_buffer) {
  csound.set_log_callback([this](csd_plugin::LogLevel level, const char *text) {
    LogMessage msg;
    msg.level = level;
    msg.source = LogSource::Csound;

    std::strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    // RT-SAFE PUSH: try_write returns 0 if the queue is full (drops message)
    (void)log_queue.try_write(1, [&](auto block1, auto block2) {
      // Since we only write 1 item, it will be in either block1 or block2
      if (!block1.empty()) {
        block1[0] = msg;
      } else {
        block2[0] = msg;
      }
    });
  });
}

void Processor::prepareToPlay(double sample_rate, int max_block_size) {
  juce::ignoreUnused(max_block_size);

  if (!sync.start_prepare_to_play()) {
    log(csd_plugin::LogLevel::Error, "Could not acquire processor stage");
    return;
  }

  ScopedStage guard(sync, ProcessorStage::PrepareToPlay);

  bool ok = csound.prepare_to_play(static_cast<int>(std::round(sample_rate)));
  if (ok && csound.is_ready_to_play()) {
    parameters.prepare(csound.get_csound(), sample_rate);
    processor_type = get_processor_type(csound.get_io_layout());
  } else {
    log(csd_plugin::LogLevel::Error, "Csound prepare_to_play failed");
  }
}

void Processor::process_no_in_no_out(juce::AudioBuffer<float> &buffer) {
  int block_size = buffer.getNumSamples();
  if (block_size <= 0) {
    return;
  }

  int csound_cycle_size = csound.get_csound_cycle_size(block_size);
  for (int index = 0; index < csound_cycle_size; ++index) {
    csound_process();
  }
}

void Processor::process_no_in_out(juce::AudioBuffer<float> &buffer) {
  const int block_size = buffer.getNumSamples();
  if (block_size <= 0) {
    return;
  }

  const int host_channels = buffer.getNumChannels();
  const auto &layout = csound.get_io_layout();
  const int out_size = layout.get_out_size();

  if (out_size <= 0) {
    buffer.clear();
    return;
  }

  csd_plugin::CsdAudioBuffers &csd_buffers = csound.get_audio_buffers();

  if (!csound.is_ready_to_play() || !csd_buffers.is_valid()) {
    buffer.clear();
    return;
  }

  for (int frame = 0; frame < block_size; ++frame) {
    if (csd_buffers.available_output_frames() == 0) {
      if (!csound_process()) {
        for (int ch = 0; ch < host_channels; ++ch) {
          buffer.clear(ch, frame, block_size - frame);
        }
        return;
      }
    }

    const bool has_output = (csd_buffers.available_output_frames() > 0);

    for (int out_ch = 0; out_ch < out_size; ++out_ch) {
      MYFLT output_sample = 0.0;

      if (has_output) {
        csd_buffers.read(output_sample);
      }

      if (out_ch < host_channels) {
        buffer.setSample(out_ch, frame, static_cast<float>(output_sample));
      }
    }

    for (int ch = out_size; ch < host_channels; ++ch) {
      buffer.setSample(ch, frame, 0.0f);
    }
  }
}

void Processor::process_in_no_out(juce::AudioBuffer<float> &buffer) {
  const int block_size = buffer.getNumSamples();
  if (block_size <= 0) {
    return;
  }

  csd_plugin::CsdAudioBuffers &csd_buffers = csound.get_audio_buffers();
  const int channel_size = buffer.getNumChannels();
  const int in_size = csound.get_io_layout().get_total_in_size();
  const int max_channel_size = std::max(in_size, channel_size);

  for (int frame_index = 0; frame_index < block_size; ++frame_index) {
    if (csd_buffers.is_full()) {
      bool is_ok = csound_process();
      if (!is_ok) {
        csd_buffers.clear();
      }
    }

    for (int channel_index = 0; channel_index < max_channel_size;
         ++channel_index) {
      if (channel_index < in_size) {
        MYFLT input_sample = (channel_index < channel_size)
                                 ? static_cast<MYFLT>(buffer.getSample(
                                       channel_index, frame_index))
                                 : 0.0;

        csd_buffers.write(input_sample);
      }

      if (channel_index < channel_size) {
        buffer.setSample(channel_index, frame_index, 0.f);
      }
    }
  }
}

void Processor::process_in_out(juce::AudioBuffer<float> &buffer) {
  const int block_size = buffer.getNumSamples();
  if (block_size <= 0)
    return;

  const int host_channels = buffer.getNumChannels();
  const auto &layout = csound.get_io_layout();
  const int in_size = layout.get_total_in_size();
  const int out_size = layout.get_out_size();
  const int ksmps = csound.get_csound_settings().ksmps;

  if (in_size <= 0 || out_size <= 0 || ksmps <= 0) {
    buffer.clear();
    return;
  }

  csd_plugin::CsdAudioBuffers &csd_buffers = csound.get_audio_buffers();

  if (!csound.is_ready_to_play() || !csd_buffers.is_valid()) {
    buffer.clear();
    return;
  }

  for (int frame = 0; frame < block_size; ++frame) {
    // If a full Csound input cycle is ready, run Csound before writing the
    // next host frame.
    //
    // This prevents input-buffer overflow and returns the previous Csound
    // cycle with the expected ksmps latency.
    if (csd_buffers.is_full()) {
      const bool process_ok = csound_process();

      if (!process_ok) {
        // If Csound cannot process, keep this block safe by clearing the
        // local Csound buffers. The host output for this frame will become
        // silence below.
        csd_buffers.clear();
      }
    }

    // Write one host frame into Csound's input buffer.
    for (int in_ch = 0; in_ch < in_size; ++in_ch) {
      MYFLT input_sample =
          (in_ch < host_channels)
              ? static_cast<MYFLT>(buffer.getSample(in_ch, frame))
              : 0.0;

      csd_buffers.write(input_sample);
    }

    // Read one host frame from Csound's output buffer if it is available.
    //
    // For FX plugins, output is delayed by ksmps samples. Before the first
    // full Csound cycle has completed, this naturally outputs silence instead
    // of uninitialized Csound memory.
    const bool has_output = (csd_buffers.available_output_frames() > 0);

    for (int out_ch = 0; out_ch < out_size; ++out_ch) {
      MYFLT output_sample = 0.0;

      if (has_output) {
        csd_buffers.read(output_sample);
      }

      if (out_ch < host_channels) {
        buffer.setSample(out_ch, frame, static_cast<float>(output_sample));
      }
    }

    // Clear extra host channels that are not used by the Csound layout.
    for (int ch = out_size; ch < host_channels; ++ch) {
      buffer.setSample(ch, frame, 0.0f);
    }
  }
}

bool Processor::csound_process() {
  parameters.update_krate_params(csound.get_csound_settings().ksmps);
  return csound.process();
}

ProcessorType
Processor::get_processor_type(const csd_plugin::IOLayout &io_layout) {
  int in_size = io_layout.get_total_in_size();
  int out_size = io_layout.get_out_size();
  if (in_size > 0) {
    if (out_size > 0) {
      return ProcessorType::InOut;
    } else {
      return ProcessorType::In;
    }
  } else {
    if (out_size > 0) {
      return ProcessorType::Out;
    } else {
      return ProcessorType::MidiOnly;
    }
  }
}

void Processor::processBlock(const juce::AudioProcessor &processor,
                             juce::AudioBuffer<float> &buffer,
                             juce::MidiBuffer &host_midi_buffer) {
  // JUCE plugins should enforce it at the entry point to
  // prevent massive CPU spikes on x86 architectures.
  const juce::ScopedNoDenormals noDenormals;

  if (!sync.start_process_block()) {
    buffer.clear();
    host_midi_buffer.clear();
    return;
  }

  ScopedStage guard(sync, ProcessorStage::ProcessBlock);

  if (!csound.is_ready_to_play()) {
    buffer.clear();
    host_midi_buffer.clear();
    return;
  }

  // Check if the host has bypassed the plugin
  bool is_bypassed = false;

  if (auto *bypass_param = processor.getBypassParameter()) {
    is_bypassed = (bypass_param->getValue() > 0.5f);
  }

  if (is_bypassed) {
    // Hard bypass for now.
    //
    // TODO:
    // For FX plugins, dry-through or crossfade bypass would be better.
    // But for FIFO stability, the important part is that we resynchronize
    // when entering/leaving bypass.
    csound.get_audio_buffers().clear();
    csound.get_midi_buffers().clear();
    buffer.clear();
    host_midi_buffer.clear();
    return;
  }

  const int block_size = buffer.getNumSamples();

  if (block_size <= 0) {
    host_midi_buffer.clear();
    return;
  }

  const int64_t block_start_global_sample = csound.get_current_sample();
  read_midi_from_host(host_midi_buffer, block_start_global_sample);
  parameters.update_inputs(processor.getPlayHead());

  switch (processor_type) {
  case juce_csd::ProcessorType::Out: {
    process_no_in_out(buffer);
    break;
  }

  case juce_csd::ProcessorType::InOut: {
    process_in_out(buffer);
    break;
  }

  case juce_csd::ProcessorType::MidiOnly: {
    process_no_in_no_out(buffer);
    break;
  }

  case juce_csd::ProcessorType::In: {
    process_in_no_out(buffer);
    break;
  }
  }

  parameters.update_outputs();
  write_midi_to_host(host_midi_buffer, block_start_global_sample, block_size);
}

void Processor::releaseResources() {
  if (!sync.start_release_resources()) {
    log(csd_plugin::LogLevel::Error,
        "Could not acquire processor stage for releaseResources");
    return;
  }

  ScopedStage guard(sync, ProcessorStage::ReleaseResources);

  csound.release_resources();
}

int Processor::get_latency_samples() { return csound.get_latency_samples(); }

void Processor::read_midi_from_host(juce::MidiBuffer &host_midi_messages,
                                    int64_t block_start_global_sample) {
  if (!csound.get_io_layout().has_midi_in) {
    return;
  }

  for (const auto metadata : host_midi_messages) {
    auto msg = metadata.getMessage();

    // Note: We filter out SysEx here
    // to guarantee 100% RT-safety (no hidden heap allocations)
    if (!msg.isSysEx()) {
      const int64_t global_pos = block_start_global_sample +
                                 static_cast<int64_t>(metadata.samplePosition);

      const int raw_size = msg.getRawDataSize();
      const uint8_t safe_size =
          static_cast<uint8_t>(std::min(raw_size, csd_plugin::MIDI_DATA_SIZE));

      if (safe_size == 0) {
        continue;
      }

      const bool pushed = csound.get_midi_buffers().in().push(
          csd_plugin::RawMidiEvent(global_pos, msg.getRawData(), safe_size));

      if (!pushed) {
        log(csd_plugin::LogLevel::Error,
            "MIDI input FIFO overflow; dropping MIDI event");
      }
    }
  }
}

void Processor::write_midi_to_host(juce::MidiBuffer &host_midi_messages,
                                   int64_t block_start_sample, int block_size) {
  host_midi_messages.clear();

  if (block_size <= 0)
    return;

  if (!csound.get_io_layout().has_midi_out)
    return;

  csd_plugin::MidiBuffer &csd_midi_buffer = csound.get_midi_buffers().out();
  const int64_t block_end_sample = block_start_sample + block_size;
  csd_plugin::RawMidiEvent csd_midi_event;

  while (csd_midi_buffer.peek(csd_midi_event)) {
    // Leave future events in the queue for the next host block.
    if (csd_midi_event.samplePosition >= block_end_sample) {
      break;
    }

    csd_midi_buffer.pop();

    if (csd_midi_event.size == 0) {
      continue;
    }

    const int64_t delta = csd_midi_event.samplePosition - block_start_sample;

    const int relative_pos =
        static_cast<int>(std::clamp<int64_t>(delta, 0, block_size - 1));

    juce::MidiMessage juce_midi_event(csd_midi_event.data, csd_midi_event.size);

    host_midi_messages.addEvent(juce_midi_event, relative_pos);
  }
}

void Processor::getStateInformation(juce::MemoryBlock &destData) {
  parameters.getStateInformation(destData);
}

void Processor::setStateInformation(const void *data, int sizeInBytes) {
  parameters.setStateInformation(data, sizeInBytes);
}

const csd_plugin::IOLayout &Processor::get_io_layout() const {
  return csound.get_io_layout();
}

Parameters &Processor::get_parameters() { return parameters; }

bool Processor::pop_log(LogMessage &msg) {
  // RT-SAFE POP: try_read returns 0 if empty
  size_t read_count = log_queue.try_read(1, [&](auto block1, auto block2) {
    if (!block1.empty()) {
      msg = block1[0];
    } else {
      msg = block2[0];
    }
  });

  return read_count > 0;
}

std::unique_ptr<CsoundLogConsumer> Processor::create_log_consumer() {
  return std::unique_ptr<CsoundLogConsumer>(new CsoundLogConsumer(*this));
}

void Processor::log(csd_plugin::LogLevel level, const char *text) {
  LogMessage msg;
  msg.level = level;
  msg.source = LogSource::Custom;

  std::strncpy(msg.text, text, sizeof(msg.text) - 1);
  msg.text[sizeof(msg.text) - 1] = '\0';

  (void)log_queue.try_write(1, [&](auto block1, auto block2) {
    if (!block1.empty()) {
      block1[0] = msg;
    } else {
      block2[0] = msg;
    }
  });
}

} // namespace juce_csd
