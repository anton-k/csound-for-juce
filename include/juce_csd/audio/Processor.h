#pragma once

#include <csound/csound.hpp>
#include <string>
#include <sys/types.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../params/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <csd_plugin/audio/Processor.h>

namespace juce_csd {

/// Class for the audio processor which runs the Csound.
// Value of this class can be used to define all methods of the plugin which relies
// on Csound audio processing.
class Processor {
  public:
    /// The processor is initialized with CSD file content, layout of IO-busses, specification of the parameters and reference to the JUCE audio processor class.
    Processor(const std::string& csd, const csd_plugin::IOLayout&, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor);
    ~Processor() { };

    /// Called on main thread to prepare plugin for audio processing
    void prepareToPlay(double sampleRate, int maxBlockSize);

    /// Called on audio-thread to process audio in blocks
    void processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>&, juce::MidiBuffer&);

    /// Called when audio processing is over to release resources
    void releaseResources();

    /// Latency of the processor in samples.
    // For plugins which process no audio input the latency is zero, and for
    // plugins which do process inputs the latency equals to ksmps.
    int get_latency_samples();

    /// Serialize the parameters of the plugin to JSON
    void getStateInformation (juce::MemoryBlock& destData);

    /// Deserialize the parameters of the plugin from JSON
    void setStateInformation (const void* data, int sizeInBytes);

    /// Get plugin parameters
    Parameters& get_parameters();

    /// Get IO-layout of the plugin
    const csd_plugin::IOLayout& get_io_layout() const;

  private:
    void read_midi_from_host(juce::MidiBuffer&);
    void write_midi_to_host(juce::MidiBuffer&, int, int);
    void read_input_buffer_from_host(juce::AudioBuffer<float>&);
    void write_output_buffer_to_host(juce::AudioBuffer<float>&);
    void update_parameters(int block_size, juce::AudioPlayHead*);

    csd_plugin::Processor csound;
    Parameters parameters;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

}
