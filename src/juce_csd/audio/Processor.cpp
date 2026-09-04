#include "csd_plugin/audio/Processor.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include "csd_plugin/audio/Logger.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_csd/audio/Processor.h>
#include <juce_csd/audio/CsoundLogConsumer.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <algorithm>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <cstring>
#include <cmath>
#include <cstdint>

namespace juce_csd {

Processor::Processor(const std::string& csd_file_content, const csd_plugin::IOLayout& io_layout, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor):
  csound(csd_file_content, io_layout), parameters(processor, parameter_spec), log_queue(log_buffer)
{
    csound.set_log_callback([this](csd_plugin::LogLevel level, const char* text) {
        LogMessage msg;
        msg.level = level;
        msg.source = LogSource::Csound;

        std::strncpy(msg.text, text, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';

        // RT-SAFE PUSH: try_write returns 0 if the queue is full (drops message)
        (void) log_queue.try_write(1, [&](auto block1, auto block2) {
            // Since we only write 1 item, it will be in either block1 or block2
            if (!block1.empty()) {
                block1[0] = msg;
            } else {
                block2[0] = msg;
            }
        });
    });
}

void Processor::prepareToPlay(double sample_rate, int max_block_size)
{
    csound.prepare_to_play(
        static_cast<int>(std::round(sample_rate)),
        max_block_size
    );

    if (!csound.is_ready_to_play()) {
        was_bypassed = false;
        return;
    }

    const int in_channels = csound.get_io_layout().get_total_in_size();
    const int out_channels = csound.get_io_layout().get_out_size();
    const int safe_block_size = std::max(0, max_block_size);

    host_input_scratch.resize(
        static_cast<size_t>(safe_block_size) *
        static_cast<size_t>(std::max(0, in_channels))
    );

    host_output_scratch.resize(
        static_cast<size_t>(safe_block_size) *
        static_cast<size_t>(std::max(0, out_channels))
    );

    parameters.prepare(csound.get_csound(), sample_rate);

    const int ksmps = csound.get_csound_settings().ksmps;

    csound.set_krate_callback([this, ksmps]() {
        // This runs inside the csd_plugin loop, once per ksmps block.
        parameters.update_krate_params(ksmps);
    });

    was_bypassed = false;
}

void Processor::processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& host_midi_buffer)
{
    // JUCE plugins should enforce it at the entry point to
    // prevent massive CPU spikes on x86 architectures.
    const juce::ScopedNoDenormals noDenormals;

    if (!csound.is_ready_to_play()) {
        buffer.clear();
        host_midi_buffer.clear();
        was_bypassed = false;
        return;
    }

    // Check if the host has bypassed the plugin
    bool is_bypassed = false;

    if (auto* bypass_param = processor.getBypassParameter()) {
        is_bypassed = (bypass_param->getValue() > 0.5f);
    }

    if (is_bypassed) {
        if (!was_bypassed) {
            csound.resync_audio_buffers();
            was_bypassed = true;
        }

        // Hard bypass for now.
        //
        // TODO:
        // For FX plugins, dry-through or crossfade bypass would be better.
        // But for FIFO stability, the important part is that we resynchronize
        // when entering/leaving bypass.
        buffer.clear();
        host_midi_buffer.clear();
        return;
    }

    if (was_bypassed) {
        csound.resync_audio_buffers();
        was_bypassed = false;
    }

    const int block_size = buffer.getNumSamples();

    if (block_size <= 0) {
        host_midi_buffer.clear();
        return;
    }

    const int in_channels = csound.get_io_layout().get_total_in_size();
    const int out_channels = csound.get_io_layout().get_out_size();

    // Safety guard: if the host gives a larger block than we prepared for,
    // do not overflow scratch buffers or FIFOs.
    const size_t needed_input_samples =
        static_cast<size_t>(block_size) * static_cast<size_t>(std::max(0, in_channels));

    const size_t needed_output_samples =
        static_cast<size_t>(block_size) * static_cast<size_t>(std::max(0, out_channels));

    if ((in_channels > 0 && host_input_scratch.size() < needed_input_samples) ||
        (out_channels > 0 && host_output_scratch.size() < needed_output_samples)) {
        csound.set_last_error("Host block size exceeds prepared scratch size; clearing");
        buffer.clear();
        host_midi_buffer.clear();
        csound.resync_audio_buffers();
        return;
    }

    const int64_t block_start_global_sample = csound.get_current_sample();

    read_input_buffer_from_host(buffer);
    read_midi_from_host(host_midi_buffer, block_start_global_sample);

    parameters.update_inputs(processor.getPlayHead());

    csound.process_block(block_size);

    parameters.update_outputs();

    write_output_buffer_to_host(buffer);
    write_midi_to_host(host_midi_buffer, block_start_global_sample, block_size);
}

void Processor::releaseResources() {
    csound.release_resources();
    was_bypassed = false;
}

int Processor::get_latency_samples() {
    return csound.get_latency_samples();
}

void Processor::read_input_buffer_from_host(juce::AudioBuffer<float>& buffer)
{
    const int csd_in_size = csound.get_io_layout().get_total_in_size();

    if (csd_in_size <= 0)
        return;

    const int host_channels = buffer.getNumChannels();
    const int num_frames = buffer.getNumSamples();
    const int total_samples = num_frames * csd_in_size;

    if (total_samples <= 0)
        return;

    MYFLT* dest = host_input_scratch.data();

    const MYFLT scale =
        static_cast<MYFLT>(csound.get_csound_settings().zero_dbfs);

    for (int frame = 0; frame < num_frames; ++frame) {
        for (int ch = 0; ch < csd_in_size; ++ch) {
            float sample = 0.0f;

            if (ch < host_channels) {
                sample = buffer.getSample(ch, frame);
            }

            *dest++ = static_cast<MYFLT>(sample) * scale;
        }
    }

    bool write_success =
        csound.get_audio_buffers().in().write_block(
            host_input_scratch.data(),
            total_samples
        );

    if (!write_success) {
        csound.set_last_error("Host input FIFO overflow; resyncing");
        csound.resync_audio_buffers();

        // Try once more after resync.
        write_success =
            csound.get_audio_buffers().in().write_block(
                host_input_scratch.data(),
                total_samples
            );

        if (!write_success) {
            csound.set_last_error("Host input FIFO overflow after resync; dropping input block");
        }
    }
}

void Processor::write_output_buffer_to_host(juce::AudioBuffer<float>& buffer)
{
    const int csd_out_size = csound.get_io_layout().get_out_size();

    if (csd_out_size <= 0)
        return;

    const int host_channels = buffer.getNumChannels();
    const int num_frames = buffer.getNumSamples();
    const int total_samples = num_frames * csd_out_size;

    if (total_samples <= 0) {
        buffer.clear();
        return;
    }

    MYFLT* src = host_output_scratch.data();

    const bool read_success =
        csound.get_audio_buffers().out().read_block(src, total_samples);

    if (!read_success) {
        csound.set_last_error("Host output FIFO underflow; clearing and resyncing");
        buffer.clear();
        csound.resync_audio_buffers();
        return;
    }

    const MYFLT inverse_scale =
        static_cast<MYFLT>(csound.get_csound_settings().inverse_zero_dbfs);

    for (int frame = 0; frame < num_frames; ++frame) {
        for (int ch = 0; ch < csd_out_size; ++ch) {
            MYFLT sample = *src++;

            sample *= inverse_scale;

            if (!std::isfinite(static_cast<double>(sample))) {
                sample = MYFLT{0};
            } else {
                sample = std::clamp(
                    sample,
                    -csd_plugin::WRAP_VOLUME_LIMIT,
                    csd_plugin::WRAP_VOLUME_LIMIT
                );
            }

            if (ch < host_channels) {
                buffer.setSample(ch, frame, static_cast<float>(sample));
            }
        }

        for (int ch = csd_out_size; ch < host_channels; ++ch) {
            buffer.setSample(ch, frame, 0.0f);
        }
    }
}

void Processor::read_midi_from_host(juce::MidiBuffer& host_midi_messages, int64_t block_start_global_sample)
{
    if (!csound.get_io_layout().has_midi_in) {
        host_midi_messages.clear();
        return;
    }

    for (const auto metadata : host_midi_messages) {
        auto msg = metadata.getMessage();

        // Note: We filter out SysEx here
        // to guarantee 100% RT-safety (no hidden heap allocations)
        if (!msg.isSysEx()) {
            const int64_t global_pos =
                block_start_global_sample +
                static_cast<int64_t>(metadata.samplePosition);

            const int raw_size = msg.getRawDataSize();
            const uint8_t safe_size =
                static_cast<uint8_t>(std::min(raw_size, csd_plugin::MIDI_DATA_SIZE));

            if (safe_size == 0) {
                continue;
            }

            const bool pushed =
                csound.get_midi_buffers().in().push(
                    csd_plugin::RawMidiEvent(
                        global_pos,
                        msg.getRawData(),
                        safe_size
                    )
                );

            if (!pushed) {
                csound.set_last_error("MIDI input FIFO overflow; dropping MIDI event");
            }
        }
    }
}

void Processor::write_midi_to_host(juce::MidiBuffer& host_midi_messages, int64_t block_start_sample, int block_size)
{
    host_midi_messages.clear();

    if (block_size <= 0)
        return;

    if (!csound.get_io_layout().has_midi_out)
        return;

    csd_plugin::MidiBuffer& csd_midi_buffer = csound.get_midi_buffers().out();

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

        juce::MidiMessage juce_midi_event(
            csd_midi_event.data,
            csd_midi_event.size
        );

        host_midi_messages.addEvent(juce_midi_event, relative_pos);
    }
}

void Processor::getStateInformation (juce::MemoryBlock& destData) {
  parameters.getStateInformation(destData);
}

void Processor::setStateInformation (const void* data, int sizeInBytes) {
  parameters.setStateInformation(data, sizeInBytes);
}

const csd_plugin::IOLayout& Processor::get_io_layout() const {
    return csound.get_io_layout();
}

Parameters& Processor::get_parameters() {
    return parameters;
}

bool Processor::pop_log(LogMessage& msg) {
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

void Processor::log(csd_plugin::LogLevel level, const char* text) {
    LogMessage msg;
    msg.level = level;
    msg.source = LogSource::Custom;

    std::strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    (void) log_queue.try_write(1, [&](auto block1, auto block2) {
        if (!block1.empty()) {
            block1[0] = msg;
        } else {
            block2[0] = msg;
        }
    });
}

}
