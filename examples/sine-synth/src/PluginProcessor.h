#pragma once

#include "csd_plugin/audio/Processor.h"
#include "juce_csd/plugin/PluginProcessor.h"
#include "const.h"
#include "EmbeddedCsd.h"

class AudioPluginAudioProcessor final : public juce_csd::PluginProcessor
{
public:
    AudioPluginAudioProcessor(): juce_csd::PluginProcessor(EmbeddedCsd::getMainCsd(), csd_plugin::IOLayout::synt(), init_parameter_spec()){};
    ~AudioPluginAudioProcessor() override {};

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
