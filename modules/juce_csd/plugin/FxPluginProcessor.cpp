#include "FxPluginProcessor.h"
#include <csound/csound.h>
#include <string>
#include "juce_core/juce_core.h"
#include "../params/Parameters.h"
#include "../audio/FxProcessor.h"

namespace juce_csd {

FxPluginProcessor::FxPluginProcessor(const std::string& csd_content, const ParameterSpec& spec)
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
                           csound(csd_content, spec, *this)

{
}

FxPluginProcessor::~FxPluginProcessor()
{
}

//==============================================================================
const juce::String FxPluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FxPluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FxPluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FxPluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
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
    csound.prepareToPlay(sampleRate);
    setLatencySamples(csound.getLatencySamples());
    juce::ignoreUnused (samplesPerBlock);
}

void FxPluginProcessor::releaseResources()
{
    csound.releaseResources();
}

bool FxPluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void FxPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    csound.processBlock(*this, buffer);
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
