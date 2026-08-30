#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <csound/csound.hpp>
#include <sys/types.h>
#include "csd_plugin/audio/Processor.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <juce_csd/params/Parameters.h>
#include <juce_csd/audio/Processor.h>

namespace juce_csd {

//==============================================================================
/// Can be used as parent to inherit from for the plugin which uses Csound for audio processing.
// It defines all methods to process audio and manages parameters. User of this class
// only needs to implement methods related to the UI (hasEditor, createEditor and also getName of the plugin).
class PluginProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    PluginProcessor(const std::string&, const csd_plugin::IOLayout& io_layout, const ParameterSpec&);
    ~PluginProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    Parameters& get_parameters() { return csound.get_parameters(); };
private:
    static juce::AudioProcessor::BusesProperties make_buses_properties(const csd_plugin::IOLayout& io_layout);
    Processor csound;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

}
