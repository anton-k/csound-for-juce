#pragma once

#include <csound/csound.hpp>
#include <memory>
#include <sys/types.h>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioBuffer.h"
#include "../params/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace juce_csd {

struct CsoundSettings {
    CsoundSettings();
    void prepare(Csound*);
    void set_channel_names(Csound*);
    int ksmps{1};
    int out_size{2};
    int in_size{0};
    float zero_dbfs{1.0}, inverse_zero_dbfs{1.0};
    std::vector<std::string> channel_names;
};

class SyntProcessor {
  public:
    SyntProcessor(const std::string& csd, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor, int buffer_size = 24000);
    ~SyntProcessor() { };

    void prepareToPlay(double sampleRate);
    void processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void releaseResources();

    void getStateInformation (juce::MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);
    Parameters& get_parameters();

    juce::AudioParameterFloat& get_param(const std::string& name);

  private:
    void clear_excess_output_channels(const juce::AudioProcessor& processor, juce::AudioBuffer<float>&);
    void write_output_buffer_to_host(juce::AudioBuffer<float>&);
    void csound_process(juce::AudioBuffer<float>&);
    void update_parameters();
    int get_csound_cycle_size(int block_size);

    std::unique_ptr<Csound> csound;
    CsoundSettings csound_settings{};
    AudioBuffers audio_buffers;
    int csound_cycle_size{0};
    std::string csd_file_content;
    Parameters parameters;
    bool is_ready_to_play;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SyntProcessor)
};

}
