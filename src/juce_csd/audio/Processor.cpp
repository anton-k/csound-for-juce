
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

namespace juce_csd {

Processor::Processor(const std::string& csd_file_content, const csd_plugin::IOLayout& io_layout, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor):
  csound(csd_file_content, io_layout), parameters(processor, parameter_spec), log_queue(log_buffer)
 {
    csound.set_log_callback([this](csd_plugin::LogLevel level, const char* text) {
        LogMessage msg;
        msg.level = level;
        std::strncpy(msg.text, text, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
        msg.source = LogSource::Csound;

        // RT-SAFE PUSH: try_write returns 0 if the queue is full (drops message)
        auto size = log_queue.try_write(1, [&](auto block1, auto block2) {
            // Since we only write 1 item, it will be in either block1 or block2
            if (!block1.empty()) {
                block1[0] = msg;
            } else {
                block2[0] = msg;
            }
        });
    });
}

void Processor::prepareToPlay (double sample_rate, int max_block_size)
{
    csound.prepare_to_play(static_cast<int>(std::round(sample_rate)),  max_block_size);
    parameters.prepare(csound.get_csound(), sample_rate, max_block_size);

    // Bind the k-rate callback
    int ksmps = csound.get_csound_settings().ksmps;
    csound.set_krate_callback([this, ksmps]() {
        // This runs inside the csd_plugin loop, once per ksmps block!
        parameters.update_krate_params(ksmps);
    });

}

void Processor::processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& host_midi_buffer)
{
    if (!csound.is_ready_to_play()) {
        buffer.clear();
        host_midi_buffer.clear();
        return;
    }

    // JUCE plugins should enforce it at the entry point to
    // prevent massive CPU spikes on x86 architectures.
    const juce::ScopedNoDenormals noDenormals;

    // Check if the host has bypassed the plugin
    bool is_bypassed = false;
    if (auto* bypass_param = processor.getBypassParameter()) {
        is_bypassed = (bypass_param->getValue() > 0.5f);
    }

    // Skip Csound processing if plugin is bypassed or csound is not ready
    if (is_bypassed || !csound.is_ready_to_play()) {
        // Safety: If not ready, clear the buffer to prevent passing garbage/previous data to the host
        buffer.clear();
        host_midi_buffer.clear();
        return;
    }

    const int block_start_global_sample = csound.get_current_sample();
    const int block_size = buffer.getNumSamples();

    read_input_buffer_from_host(buffer);
    read_midi_from_host(host_midi_buffer, block_start_global_sample);
    update_parameters(processor.getPlayHead());
    csound.process_block(block_size);
    write_output_buffer_to_host(buffer);
    write_midi_to_host(host_midi_buffer, block_start_global_sample, block_size);
}

void Processor::releaseResources() {
    csound.release_resources();
}

int Processor::get_latency_samples() {
    return csound.get_latency_samples();
}

void Processor::read_input_buffer_from_host(juce::AudioBuffer<float>& buffer) {
    int csd_in_size = csound.get_io_layout().get_total_in_size();
    if (csd_in_size > 0) {
        int host_in_size = buffer.getNumChannels();
        int sample_size = buffer.getNumSamples();

        for (int sample_index = 0; sample_index < sample_size; ++sample_index) {
            for (int channel_index = 0; channel_index < csd_in_size; ++channel_index) {
                if (channel_index < host_in_size) {
                    csound.write_input(static_cast<double>(buffer.getSample(channel_index, sample_index)));
                } else {
                    csound.write_input(0.f); // Pad missing host channels with zeroes
                }
            }
        }
    }

}

void Processor::write_output_buffer_to_host(juce::AudioBuffer<float>& buffer) {
    int csd_out_size = csound.get_io_layout().get_out_size();
    int host_out_size = buffer.getNumChannels();
    int sample_size = buffer.getNumSamples();
    int max_channels = std::max(csd_out_size, host_out_size);

    double sample{0.0};
    bool read_success;
    for (int sample_index = 0; sample_index < sample_size; ++sample_index) {
        for (int channel_index = 0; channel_index < max_channels; ++channel_index) {
            if (channel_index < csd_out_size) {
                read_success = csound.read_output(sample);
                if (!read_success) {
                    sample = 0.0; // Fallback to silence on underflow
                }
            } else {
                sample = 0.0;
            }
            if (channel_index < host_out_size) {
                buffer.setSample(channel_index, sample_index, static_cast<float>(sample));
            }
        }
    }
}

void Processor::read_midi_from_host(juce::MidiBuffer& host_midi_messages, int block_start_global_sample) {
    if (csound.get_io_layout().has_midi_in) {
        // Note: We filter out SysEx here
        // to guarantee 100% RT-safety (no hidden heap allocations)
        for (const auto metadata : host_midi_messages) {
            auto msg = metadata.getMessage();
            if (!msg.isSysEx()) {
                int32_t global_pos = block_start_global_sample + metadata.samplePosition;
                csound.get_midi_buffers().in().push(csd_plugin::RawMidiEvent(global_pos, msg.getRawData(), msg.getRawDataSize()));
            }
        }
    }
}

void Processor::write_midi_to_host(juce::MidiBuffer& host_midi_messages, int block_start_sample, int block_size) {
    if (block_size <= 0) return;
    if (csound.get_io_layout().has_midi_out) {
        host_midi_messages.clear();
        csd_plugin::RawMidiEvent csd_midi_event;
        csd_plugin::MidiBuffer& csd_midi_buffer = csound.get_midi_buffers().out();

        while (csd_midi_buffer.read(csd_midi_event)) {
            juce::MidiMessage juce_midi_event(csd_midi_event.data, csd_midi_event.size);
            int relative_pos = csd_midi_event.samplePosition - block_start_sample;
            relative_pos = std::clamp(relative_pos, 0, block_size - 1);
            host_midi_messages.addEvent(juce_midi_event, relative_pos);
        }
    }
}


void Processor::update_parameters(juce::AudioPlayHead* play_head) {
    parameters.update_on_process(play_head);
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
    msg.source = LogSource::Custom; // <-- Tagged
    std::strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    auto size = log_queue.try_write(1, [&](auto block1, auto block2) {
        if (!block1.empty()) block1[0] = msg; else block2[0] = msg;
    });
}
}
