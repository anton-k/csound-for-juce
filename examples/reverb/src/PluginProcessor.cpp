#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::string& csdContent = EmbeddedCsd::getReverbCsd();

/// Our plugin has UI editor so we return true
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

/// Let's create the UI editor.
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

/// Returns the name of the plugin (we use predefined JUCE macro
// which reads name from the cmake file)
const juce::String AudioPluginAudioProcessor::getName() const {
    return JucePlugin_Name;
}

/// Main entry point to create JUCE plugin.
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

