
#include "csd_plugin/audio/Processor.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_csd/audio/Processor.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <memory>
#include <ranges>
#include <juce_audio_processors/juce_audio_processors.h>

const float WRAP_VOLUME_LIMIT = 5.0f;

namespace juce_csd {

namespace {

float wrap_limiter(float sample) {
    if (std::abs(sample) > WRAP_VOLUME_LIMIT) {
        return WRAP_VOLUME_LIMIT * std::signbit(sample);
    } else {
        return sample;
    }
}

bool is_control_channel_type(controlChannelInfo_t info) {
  return (info.type & CSOUND_CHANNEL_TYPE_MASK) == CSOUND_CONTROL_CHANNEL;
}

}

Processor::Processor(const std::string& csd_file_content, const csd_plugin::IOLayout& io_layout, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor):
  csound(csd_file_content, io_layout), parameters(processor, parameter_spec)
 {}

void Processor::prepareToPlay (double sample_rate, int max_block_size)
{
    csound.prepare_to_play(std::ceil(sample_rate),  max_block_size);
}

void Processor::processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& host_midi_buffer)
{
    if (csound.is_ready_to_play()) {
        read_input_buffer_from_host(buffer);
        read_midi_from_host(host_midi_buffer);
        update_parameters();
        csound.process_block(buffer.getNumSamples());
        write_output_buffer_to_host(buffer);
        write_midi_to_host(host_midi_buffer);
    }
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

        for (auto sample_index: std::ranges::iota_view(0, sample_size)) {
            for (auto channel_index: std::ranges::iota_view(0, csd_in_size)) {
                if (channel_index < host_in_size) {
                    csound.write_input(buffer.getSample(channel_index, sample_index));
                } else {
                    csound.write_input(0.f);
                }
            }
        }
    }

}

void Processor::write_output_buffer_to_host(juce::AudioBuffer<float>& buffer) {
    int csd_out_size = csound.get_io_layout().get_out_size();
    int host_out_size = buffer.getNumChannels();
    int sample_size = buffer.getNumSamples();

    float sample{0.0};
    for (auto sample_index: std::ranges::iota_view(0, sample_size)) {
        for (auto channel_index: std::ranges::iota_view(0, host_out_size)) {
            if (channel_index < csd_out_size) {
                csound.read_output(sample);
            } else {
                sample = 0.f;
            }
            buffer.setSample(channel_index, sample_index, sample);
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

void Processor::write_midi_to_host(juce::MidiBuffer& host_midi_messages) {
    if (csound.get_io_layout().has_midi_out) {
        host_midi_messages.clear();
        csd_plugin::RawMidiEvent csd_midi_event;
        csd_plugin::MidiBuffer& csd_midi_buffer = csound.get_midi_buffers().out();
        while (csd_midi_buffer.read(csd_midi_event)) {
            juce::MidiMessage juce_midi_event(csd_midi_event.data, csd_midi_event.size);
            host_midi_messages.addEvent(juce_midi_event, csd_midi_event.samplePosition);
        }

    }
}

void Processor::update_parameters() {
  parameters.update_on_process(csound.get_csound().get());
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
