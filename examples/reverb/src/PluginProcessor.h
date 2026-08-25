#pragma once

#include "csd_plugin/audio/Processor.h"
#include "juce_csd/plugin/PluginProcessor.h"
#include "const.h"
#include "EmbeddedCsd.h"
const std::string& csdContent = EmbeddedCsd::getReverbCsd();

class AudioPluginAudioProcessor final : public juce_csd::PluginProcessor
{
public:
    AudioPluginAudioProcessor(): juce_csd::PluginProcessor(csdContent, csd_plugin::IOLayout::fx_layout(), init_parameter_spec()){};
    ~AudioPluginAudioProcessor() override {};

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
