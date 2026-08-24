#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <csound/csound.hpp>
#include <sys/types.h>
#include "juce_audio_basics/juce_audio_basics.h"
#include <juce_csd/params/Parameters.h>
#include <juce_csd/audio/Processor.h>

namespace juce_csd {

//==============================================================================
class FxPluginProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    FxPluginProcessor(const std::string&, const ParameterSpec&);
    ~FxPluginProcessor() override;

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
    Processor csound;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxPluginProcessor)
};

}
