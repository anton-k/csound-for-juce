#include "PluginProcessor.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "PluginEditor.h"
#include "const.h"
#include <juce_core/juce_core.h>
#include <juce_csd/params/Parameters.h>
#include <juce_csd/audio/CsoundLogConsumer.h>

using namespace juce_csd;

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), font(juce::FontOptions(24.0)), parameter_attachments(p.get_parameters()),
    constrainer(new juce::ComponentBoundsConstrainer())
{
    // Inits knobs to control float audio parameters
    for (auto* knob : {&size_knob, &tone_knob, &mix_knob}) {
        knob->setSliderStyle(juce::Slider::Rotary);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        knob->setPopupDisplayEnabled(true, true, this);
        knob->setRange(0.0, 1.0, 0.01);

        addAndMakeVisible(knob);
    }

    // Choice parameter definition. Note that parameter index is 1-based. Because 0 is
    // reserved in the JUCE for "no selection".
    reverb_type_selector.addItem("Sean Costello", 1);
    reverb_type_selector.addItem("Freeverb", 2);
    reverb_type_selector.addItem("Nverb", 3);
    addAndMakeVisible(reverb_type_selector);

    /// Labels for the knobs
    size_label.setText(names::size, juce::NotificationType::dontSendNotification);
    tone_label.setText(names::tone, juce::NotificationType::dontSendNotification);
    mix_label.setText(names::mix, juce::NotificationType::dontSendNotification);
    for (auto* label : {&size_label, &tone_label, &mix_label}) {
        label->setJustificationType(juce::Justification::horizontallyCentred);
        label->setFont(font);
        addAndMakeVisible(label);
    }

    /// Setup height and width fir the UI window (those params are persisted in the plugin state)
    int window_height = static_cast<int>(processorRef.get_parameters().get_ui_parameter(names::window_height).value_or(250.0));
    int window_width = static_cast<int>(processorRef.get_parameters().get_ui_parameter(names::window_width).value_or(400.0));
    setSize (window_width, window_height);
    setResizable(true, true);

    /// Setup constrainer to keep fixed ratio of the window width and height
    constrainer->setFixedAspectRatio(static_cast<float>(window_width) / static_cast<float>(window_height));
    constrainer->setMinimumHeight(100);
    constrainer->setMinimumWidth(200);
    setConstrainer(constrainer.get());

    /// Setup parameter attachments. It links UI-control to the update of Csound parameters
    parameter_attachments.add_slider(names::size, size_knob);
    parameter_attachments.add_slider(names::tone, tone_knob);
    parameter_attachments.add_slider(names::mix, mix_knob);
    parameter_attachments.add_combo_box(names::reverb_type, reverb_type_selector);


    log_consumer = processorRef.create_log_consumer();
    auto log_dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("MyPluginLogs");
    log_dir.createDirectory();
    log_consumer->enable_file_logging(log_dir.getChildFile("csound_log.txt"), "Plugin Started");

    // 2. Register UI callback for error banners
    log_consumer->set_ui_callback([this](const juce::String& msg, csd_plugin::LogLevel level) {
        juce::ignoreUnused(msg);
        juce::ignoreUnused(this);
        if (level == csd_plugin::LogLevel::Error) {
            // e.g., errorBanner.setVisible(true);
            // errorLabel.setText(msg, juce::dontSendNotification);
        }
    });

    log_consumer->start_consuming(30);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    log_consumer->stop_consuming();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

int scale_int(float ratio, int value) {
    return round(ratio * value);
}

/// Defines layout of the plugin UI
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
    int label_y = scale_int(140.0 / values::window_height, local_height);
    int selector_x = scale_int(0.15, local_width) + second_knob_x;
    int selector_y = scale_int(195.0 / values::window_height, local_height);
    int selector_height = scale_int(30.0 / values::window_width, local_width);
    int selector_width = scale_int(0.75, knob_width * 2 + pad_x);

    size_knob.setBounds({ pad_x, pad_y, knob_width, knob_height });
    tone_knob.setBounds({ second_knob_x, pad_y, knob_width, knob_height });
    mix_knob.setBounds({ third_knob_x, pad_y, knob_width, knob_height });
    size_label.setBounds({ pad_x, label_y, knob_width, pad_y });
    tone_label.setBounds({ second_knob_x, label_y, knob_width, pad_y });
    mix_label.setBounds({ third_knob_x, label_y, knob_width, pad_y });
    processorRef.get_parameters().set_ui_parameter(names::window_width, local_width);
    processorRef.get_parameters().set_ui_parameter(names::window_height, local_height);

    reverb_type_selector.setBounds({selector_x, selector_y, selector_width, selector_height});
}
