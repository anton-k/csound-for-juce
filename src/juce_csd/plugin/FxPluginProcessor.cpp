#include <juce_csd/plugin/FxPluginProcessor.h>
#include <csound/csound.h>
#include <string>
#include "juce_core/juce_core.h"
#include <juce_csd/params/Parameters.h>
#include <juce_csd/audio/Processor.h>
#include <csd_plugin/audio/Processor.h>

namespace juce_csd {

FxPluginProcessor::FxPluginProcessor(const std::string& csd_content, const ParameterSpec& spec)
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       ),
                           csound(csd_content, csd_plugin::IOLayout::fx_layout(), spec, *this)

{
}

FxPluginProcessor::~FxPluginProcessor()
{
}

//==============================================================================

bool FxPluginProcessor::acceptsMidi() const
{
    return false;
}

bool FxPluginProcessor::producesMidi() const
{
    return false;
}

bool FxPluginProcessor::isMidiEffect() const
{
    return false;
}

double FxPluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FxPluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FxPluginProcessor::getCurrentProgram()
{
    return 0;
}

void FxPluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String FxPluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void FxPluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void FxPluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    csound.prepareToPlay(sampleRate, samplesPerBlock);
    setLatencySamples(csound.get_latency_samples());
    juce::ignoreUnused (samplesPerBlock);
}

void FxPluginProcessor::releaseResources()
{
    csound.releaseResources();
}

bool FxPluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
     && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void FxPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    csound.processBlock(*this, buffer, midiMessages);
}

//==============================================================================
void FxPluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    csound.getStateInformation(destData);
}

void FxPluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    csound.setStateInformation(data, sizeInBytes);
}

}
