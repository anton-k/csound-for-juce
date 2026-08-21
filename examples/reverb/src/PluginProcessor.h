#pragma once

#include "juce_csd/plugin/FxPluginProcessor.h"
#include "const.h"
#include "embedded_data.h"

class AudioPluginAudioProcessor final : public juce_csd::FxPluginProcessor
{
public:
    AudioPluginAudioProcessor(): juce_csd::FxPluginProcessor(std::string(getEmbeddedCsd()), init_parameter_spec()){};
    ~AudioPluginAudioProcessor() override {};

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
