#pragma once

#include "csd_plugin/audio/Processor.h"
#include "juce_csd/plugin/PluginProcessor.h"
#include "const.h"
#include "EmbeddedCsd.h"

/// To implement our plugin which is based on Csound we inherit from the PluginProcessor class.
// After that we have only 3 methods to implement. getName - to get the plugin name,
// hasEditor - to tell to the host that plugin has UI. And createEditor to create UI.
class AudioPluginAudioProcessor final : public juce_csd::PluginProcessor
{
public:
    AudioPluginAudioProcessor(): juce_csd::PluginProcessor(EmbeddedCsd::getReverbCsd(), csd_plugin::IOLayout::fx(), init_parameter_spec()){};
    ~AudioPluginAudioProcessor() override {};

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
