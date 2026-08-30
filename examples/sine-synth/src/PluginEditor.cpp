#include "PluginProcessor.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "PluginEditor.h"
#include "const.h"
#include <juce_csd/params/Parameters.h>

using namespace juce_csd;

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), font(juce::FontOptions(24.0)), parameter_attachments(p.get_parameters()),
    constrainer(new juce::ComponentBoundsConstrainer())
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    for (auto* knob : {&attack_knob, &decay_knob, &sustain_knob, &release_knob, &gain_knob}) {
        knob->setSliderStyle(juce::Slider::Rotary);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        knob->setPopupDisplayEnabled(true, true, this);
        knob->setRange(0.0, 1.0, 0.01);

        addAndMakeVisible(knob);
    }


    attack_label.setText(names::attack, juce::NotificationType::dontSendNotification);
    decay_label.setText(names::decay, juce::NotificationType::dontSendNotification);
    sustain_label.setText(names::sustain, juce::NotificationType::dontSendNotification);
    release_label.setText(names::release, juce::NotificationType::dontSendNotification);
    gain_label.setText(names::gain, juce::NotificationType::dontSendNotification);
    for (auto* label : {&attack_label, &decay_label, &sustain_label, &release_label, &gain_label}) {
        label->setJustificationType(juce::Justification::horizontallyCentred);
        label->setFont(font);
        addAndMakeVisible(label);
    }
    int window_height = static_cast<int>(processorRef.get_parameters().get_ui_parameter(names::window_height).value_or(250.0));
    int window_width = static_cast<int>(processorRef.get_parameters().get_ui_parameter(names::window_width).value_or(400.0));
    setSize (window_width, window_height);
    setResizable(true, true);
    constrainer->setFixedAspectRatio(static_cast<float>(window_width) / static_cast<float>(window_height));
    constrainer->setMinimumHeight(100);
    constrainer->setMinimumWidth(200);
    setConstrainer(constrainer.get());

    parameter_attachments.add_slider(names::attack, attack_knob);
    parameter_attachments.add_slider(names::decay, decay_knob);
    parameter_attachments.add_slider(names::sustain, sustain_knob);
    parameter_attachments.add_slider(names::release, release_knob);
    parameter_attachments.add_slider(names::gain, gain_knob);

}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

int scale_int(float ratio, int value) {
    return round(ratio * value);
}

void AudioPluginAudioProcessorEditor::resized()
{
    juce::Rectangle<int> local_bounds = getLocalBounds();
    int local_width = local_bounds.getWidth();
    int local_height = local_bounds.getHeight();

    int pad_x = scale_int(30.0 / values::window_width, local_width);
    int pad_y = scale_int(30.0 / values::window_height, local_height);
    int knob_width = scale_int(100.0 / values::window_width, local_width);
    int knob_height = scale_int(100.0/ values::window_height, local_height);
    int pad_knob = scale_int(20.0 / values::window_width, local_width);

    int second_knob_x = pad_x + knob_width + pad_knob;
    int third_knob_x = second_knob_x + knob_width + pad_knob;
    int fourth_knob_x = third_knob_x + knob_width + pad_knob;
    int fifth_knob_x = fourth_knob_x + knob_width + pad_knob;
    int label_y = scale_int(140.0 / values::window_height, local_height);

    attack_knob.setBounds({ pad_x, pad_y, knob_width, knob_height });
    decay_knob.setBounds({ second_knob_x, pad_y, knob_width, knob_height });
    sustain_knob.setBounds({ third_knob_x, pad_y, knob_width, knob_height });
    release_knob.setBounds({ fourth_knob_x, pad_y, knob_width, knob_height });
    gain_knob.setBounds({ fifth_knob_x, pad_y, knob_width, knob_height });
    attack_label.setBounds({ pad_x, label_y, knob_width, pad_y });
    decay_label.setBounds({ second_knob_x, label_y, knob_width, pad_y });
    sustain_label.setBounds({ third_knob_x, label_y, knob_width, pad_y });
    release_label.setBounds({ fourth_knob_x, label_y, knob_width, pad_y });
    gain_label.setBounds({ fifth_knob_x, label_y, knob_width, pad_y });
    processorRef.get_parameters().set_ui_parameter(names::window_width, local_width);
    processorRef.get_parameters().set_ui_parameter(names::window_height, local_height);
}
