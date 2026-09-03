#pragma once

#include "PluginProcessor.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <memory>
#include <juce_csd/params/Parameters.h>

using namespace juce_csd;

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& processorRef;
    juce::Slider size_knob, tone_knob, mix_knob;
    juce::ComboBox reverb_type_selector;
    juce::Label size_label, tone_label, mix_label;
    juce::Font font;
    juce_csd::ParameterAttachments parameter_attachments;
    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;
    std::unique_ptr<juce_csd::CsoundLogConsumer> log_consumer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
