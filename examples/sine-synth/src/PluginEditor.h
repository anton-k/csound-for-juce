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
    juce::Slider attack_knob, decay_knob, sustain_knob, release_knob, gain_knob;
    juce::Label attack_label, decay_label, sustain_label, release_label, gain_label;
    juce::Font font;
    juce_csd::ParameterAttachments parameter_attachments;
    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
