#pragma once

#include "juce_csd/plugin/FxPluginProcessor.h"
#include "const.h"
#include "EmbeddedCsd.h"
const std::string& csdContent = EmbeddedCsd::getReverbCsd();

class AudioPluginAudioProcessor final : public juce_csd::FxPluginProcessor
{
public:
    AudioPluginAudioProcessor(): juce_csd::FxPluginProcessor(csdContent, init_parameter_spec()){};
    ~AudioPluginAudioProcessor() override {};

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
