#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_csd/plugin/PluginProcessor.h>
#include <csound/csound.h>
#include <juce_events/juce_events.h>
#include <string>
#include "juce_core/juce_core.h"
#include <juce_csd/params/Parameters.h>
#include <juce_csd/audio/Processor.h>
#include <csd_plugin/audio/Processor.h>

namespace juce_csd {


juce::AudioProcessor::BusesProperties PluginProcessor::make_buses_properties(const csd_plugin::IOLayout& io_layout) {
    BusesProperties bp;

    switch (io_layout.in_size) {
        case 1: bp.addBus(true, "Input", juce::AudioChannelSet::mono(), true); break;
        case 2: bp.addBus(true, "Input", juce::AudioChannelSet::stereo(), true); break;
    }

    switch (io_layout.out_size) {
        case 1: bp.addBus(true, "Output", juce::AudioChannelSet::mono(), true); break;
        case 2: bp.addBus(true, "Output", juce::AudioChannelSet::stereo(), true); break;
    }

    if (!juce::JUCEApplicationBase::isStandaloneApp()) {
        if (io_layout.has_sidechain) {
            bp.addBus(true, "Sidechain", juce::AudioChannelSet::stereo(), false);

        }
    }

    return bp;
}

PluginProcessor::PluginProcessor(const std::string& csd_content, const csd_plugin::IOLayout& io_layout, const ParameterSpec& spec)
     : AudioProcessor (PluginProcessor::make_buses_properties(io_layout)),
       csound(csd_content, io_layout, spec, *this)
{
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================

bool PluginProcessor::acceptsMidi() const
{
    return false;
}

bool PluginProcessor::producesMidi() const
{
    return false;
}

bool PluginProcessor::isMidiEffect() const
{
    return false;
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    csound.prepareToPlay(sampleRate, samplesPerBlock);
    setLatencySamples(csound.get_latency_samples());
    juce::ignoreUnused (samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    csound.releaseResources();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
     && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    csound.processBlock(*this, buffer, midiMessages);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    csound.getStateInformation(destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    csound.setStateInformation(data, sizeInBytes);
}

}
