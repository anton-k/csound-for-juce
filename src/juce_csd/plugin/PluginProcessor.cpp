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
        case 1: bp.addBus(false, "Output", juce::AudioChannelSet::mono(), true); break;
        case 2: bp.addBus(false, "Output", juce::AudioChannelSet::stereo(), true); break;
    }

    // Standalon JUCE app has no support for sidechain inputs
    if (!juce::JUCEApplicationBase::isStandaloneApp()) {
        if (io_layout.sidechain_size > 0) {
            switch (io_layout.sidechain_size) {
                case 1: bp.addBus(true, "Sidechain", juce::AudioChannelSet::mono(), true); break;
                case 2: bp.addBus(true, "Sidechain", juce::AudioChannelSet::stereo(), true); break;
            }
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
    return csound.get_io_layout().has_midi_in;
}

bool PluginProcessor::producesMidi() const
{
    return csound.get_io_layout().has_midi_out;
}

bool PluginProcessor::isMidiEffect() const
{
    int in_size = csound.get_io_layout().get_total_in_size();
    int out_size = csound.get_io_layout().get_out_size();
    bool has_midi_in = csound.get_io_layout().has_midi_in;
    bool has_midi_out = csound.get_io_layout().has_midi_out;

    return has_midi_in && has_midi_out && in_size == 0 && out_size == 0;
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
    auto layout = csound.get_io_layout();

    return (layouts.getMainOutputChannelSet().size() == layout.get_out_size()
     && (layouts.getMainInputChannelSet().size() == layout.get_total_in_size() ||
         layouts.getMainInputChannelSet().size() == layout.in_size
        )

    );
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
