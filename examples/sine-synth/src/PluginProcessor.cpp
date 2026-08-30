#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::string& csdContent = EmbeddedCsd::getMainCsd();


bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

const juce::String AudioPluginAudioProcessor::getName() const {
    return JucePlugin_Name;
}


