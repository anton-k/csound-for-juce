#pragma once

#include "PluginProcessor.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <memory>
#include <juce_csd/params/Parameters.h>

using namespace juce_csd;

//==============================================================================

/// UI for the plugin
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;


private:
    /// reference to the audio processor (to access parameters and create parameter attachments)
    AudioPluginAudioProcessor& processorRef;

    /// knobs to control float parameters
    juce::Slider attack_knob, decay_knob, sustain_knob, release_knob, gain_knob;

    /// parameter labels
    juce::Label attack_label, decay_label, sustain_label, release_label, gain_label;

    /// font for the labels
    juce::Font font;

    /// parameter attachments to link between UI-controls and Csound parameters
    juce_csd::ParameterAttachments parameter_attachments;

    //// Window resize constrainer. We use it to keep the fixed ratio of window width and height.
    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
