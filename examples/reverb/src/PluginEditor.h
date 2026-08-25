#pragma once

#include "PluginProcessor.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_csd/params/Parameters.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <memory>

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
    juce::Label size_label, tone_label, mix_label;
    juce::Font font;
    ParameterAttachments parameter_attachments;
    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
