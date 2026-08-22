#pragma once

#include <csound/csound.hpp>
#include <memory>
#include <sys/types.h>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioBuffer.h"
#include "../params/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_csd/audio/MidiBuffer.h>

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
    void read_midi_from_host(juce::MidiBuffer&);
    void write_output_buffer_to_host(juce::AudioBuffer<float>&);
    void csound_process(juce::AudioBuffer<float>&);
    void update_parameters();
    int get_csound_cycle_size(int block_size);
    void setup_csound(double sampleRate);
    void set_csound_midi_callbacks();
    void clear_buffers();

    // midi callbacks
    static int midi_read(CSOUND*, void* userData, unsigned char* buf, int n);
    static int midi_write(CSOUND *csound_, void *userData, const unsigned char *midi_buffer, int midi_buffer_size);
    static int midi_device_open(CSOUND *csound_, void **user_data, const char *devName);
    static int midi_device_close(CSOUND *csound_, void *user_data);

    std::unique_ptr<Csound> csound;
    CsoundSettings csound_settings{};
    AudioBuffers audio_buffers;
    MidiBuffer midi_buffer;
    int csound_cycle_size{0};
    std::string csd_file_content;
    Parameters parameters;
    bool is_ready_to_play;
    int current_sample_end_sample{0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SyntProcessor)
};

}
