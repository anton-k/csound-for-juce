
#include "csd_plugin/audio/Processor.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_csd/audio/Processor.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <algorithm>
#include <juce_audio_processors/juce_audio_processors.h>

namespace juce_csd {

Processor::Processor(const std::string& csd_file_content, const csd_plugin::IOLayout& io_layout, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor):
  csound(csd_file_content, io_layout), parameters(processor, parameter_spec)
 {}

void Processor::prepareToPlay (double sample_rate, int max_block_size)
{
    parameters.prepare(sample_rate, max_block_size);
    csound.prepare_to_play(static_cast<int>(std::round(sample_rate)),  max_block_size);
}

void Processor::processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& host_midi_buffer)
{
    if (!csound.is_ready_to_play()) {
        // Safety: If not ready, clear the buffer to prevent passing garbage/previous data to the host
        buffer.clear();
        host_midi_buffer.clear();
        return;
    }

    const int block_start_global_sample = csound.get_current_sample();
    const int block_size = buffer.getNumSamples();

    read_input_buffer_from_host(buffer);
    read_midi_from_host(host_midi_buffer);
    update_parameters();
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
    int csd_in_size = csound.get_io_layout().get_in_size();
    if (csd_in_size > 0) {
        int host_in_size = buffer.getNumChannels();
        int sample_size = buffer.getNumSamples();

        for (int sample_index = 0; sample_index < sample_size; ++sample_index) {
            for (int channel_index = 0; channel_index < csd_in_size; ++channel_index) {
                if (channel_index < host_in_size) {
                    csound.write_input(buffer.getSample(channel_index, sample_index));
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

    float sample{0.0};
    for (int sample_index = 0; sample_index < sample_size; ++sample_index) {
        for (int channel_index = 0; channel_index < max_channels; ++channel_index) {
            if (channel_index < csd_out_size) {
                csound.read_output(sample);
            } else {
                sample = 0.f;
            }
            if (channel_index < host_out_size) {
                buffer.setSample(channel_index, sample_index, sample);
            }
        }
    }
}

void Processor::read_midi_from_host(juce::MidiBuffer& host_midi_messages) {
    if (csound.get_io_layout().has_midi_in) {
        // Note: We filter out SysEx here
        // to guarantee 100% RT-safety (no hidden heap allocations)
        for (const auto metadata : host_midi_messages) {
            auto msg = metadata.getMessage();
            if (!msg.isSysEx()) {
                csound.get_midi_buffers().in().push(csd_plugin::RawMidiEvent(metadata.samplePosition, msg.getRawData(), msg.getRawDataSize()));
            }
        }
    }
}

void Processor::write_midi_to_host(juce::MidiBuffer& host_midi_messages, int block_start_sample, int block_size) {
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


void Processor::update_parameters() {
    Csound* csound_ptr = csound.get_csound();
    if (csound_ptr != nullptr) {
        parameters.update_on_process(csound_ptr);
    }
}

void Processor::getStateInformation (juce::MemoryBlock& destData) {
  parameters.getStateInformation(destData);
}

void Processor::setStateInformation (const void* data, int sizeInBytes) {
  parameters.setStateInformation(data, sizeInBytes);
}

juce::AudioParameterFloat& Processor::get_parameter(const std::string& name) {
    return parameters.get_audio_parameter_ref(name);
}

Parameters& Processor::get_parameters() {
    return parameters;
}
}
