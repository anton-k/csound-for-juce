#pragma once

#include "PluginProcessor.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_csd/plugin/PluginProcessor.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <juce_csd/params/Parameters.h>
#include <juce_csd/ui/ErrorBanner.h>
#include <juce_csd/audio/CsoundLogConsumer.h>

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (juce_csd::PluginProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setup_window();
    void setup_knobs();
    void setup_reverb_type_choice();
    void setup_logger();
    void setup_parameter_attachments();

    juce_csd::PluginProcessor& processorRef;
    juce::Slider size_knob, tone_knob, mix_knob;
    juce::ComboBox reverb_type_selector;
    juce::Label size_label, tone_label, mix_label;
    juce::Font font = juce::Font(juce::FontOptions(24.0f));
    juce_csd::ParameterAttachments parameter_attachments;
    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;
    bool error_popup_shown = false;
    juce_csd::ErrorBanner error_banner;

    // Log consumer must be declared last so it is destroyed first,
    // preventing timer callbacks from firing on destroyed UI components.
    std::unique_ptr<juce_csd::CsoundLogConsumer> log_consumer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
