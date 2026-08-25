#include "PluginProcessor.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "PluginEditor.h"
#include "const.h"

using namespace juce_csd;

//==============================================================================

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), font(juce::FontOptions(24.0)), parameter_attachments(p.get_parameters()),
    constrainer(new juce::ComponentBoundsConstrainer())
{
    parameter_attachments.add_slider(names::size, size_knob);
    parameter_attachments.add_slider(names::tone, tone_knob);
    parameter_attachments.add_slider(names::mix, mix_knob);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    for (auto* knob : {&size_knob, &tone_knob, &mix_knob}) {
        knob->setSliderStyle(juce::Slider::Rotary);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        knob->setPopupDisplayEnabled(true, true, this);
        knob->setRange(0.0, 1.0, 0.01);

        addAndMakeVisible(knob);
    }

    size_label.setText(names::size, juce::NotificationType::dontSendNotification);
    tone_label.setText(names::tone, juce::NotificationType::dontSendNotification);
    mix_label.setText(names::mix, juce::NotificationType::dontSendNotification);
    for (auto* label : {&size_label, &tone_label, &mix_label}) {
        label->setJustificationType(juce::Justification::horizontallyCentred);
        label->setFont(font);
        addAndMakeVisible(label);
    }
    int window_height = static_cast<int>(processorRef.get_parameters().get_ui_parameter(names::window_height).value_or(200.0));
    int window_width = static_cast<int>(processorRef.get_parameters().get_ui_parameter(names::window_width).value_or(400.0));
    setSize (window_width, window_height);
    setResizable(true, true);
    constrainer->setFixedAspectRatio(static_cast<float>(window_width) / static_cast<float>(window_height));
    constrainer->setMinimumHeight(100);
    constrainer->setMinimumWidth(200);
    setConstrainer(constrainer.get());
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

    int pad_x = scale_int(30.0 / 400.0, local_width);
    int pad_y = scale_int(30.0 / 200.0, local_height);
    int knob_width = scale_int(100.0 / 400.0, local_width);
    int knob_height = scale_int(100.0/ 200.0, local_height);
    int pad_knob = scale_int(20.0 / 400.0, local_width);

    int second_knob_x = pad_x + knob_width + pad_knob;
    int third_knob_x = second_knob_x + knob_width + pad_knob;
    int label_y = scale_int(140.0 / 200.0, local_height);

    size_knob.setBounds({ pad_x, pad_y, knob_width, knob_height });
    tone_knob.setBounds({ second_knob_x, pad_y, knob_width, knob_height });
    mix_knob.setBounds({ third_knob_x, pad_y, knob_width, knob_height });
    size_label.setBounds({ pad_x, label_y, knob_width, pad_y });
    tone_label.setBounds({ second_knob_x, label_y, knob_width, pad_y });
    mix_label.setBounds({ third_knob_x, label_y, knob_width, pad_y });
    processorRef.get_parameters().set_ui_parameter(names::window_width, local_width);
    processorRef.get_parameters().set_ui_parameter(names::window_height, local_height);
}
