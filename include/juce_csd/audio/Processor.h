#pragma once

#include <csound/csound.hpp>
#include <string>
#include <sys/types.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../params/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <csd_plugin/audio/Processor.h>

namespace juce_csd {

class Processor {
  public:
    Processor(const std::string& csd, const csd_plugin::IOLayout&, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor);
    ~Processor() { };

    void prepareToPlay(double sampleRate, int maxBlockSize);
    void processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void releaseResources();
    int get_latency_samples();

    void getStateInformation (juce::MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);
    Parameters& get_parameters();
    juce::AudioParameterFloat& get_parameter(const std::string& name);

  private:
    void read_midi_from_host(juce::MidiBuffer&);
    void write_midi_to_host(juce::MidiBuffer&);
    void read_input_buffer_from_host(juce::AudioBuffer<float>&);
    void write_output_buffer_to_host(juce::AudioBuffer<float>&);
    void update_parameters();

    csd_plugin::Processor csound;
    Parameters parameters;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

}
