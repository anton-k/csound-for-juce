# Quickstart guide: building a synthesizer

In the previous tutorial we have learned how to build an effect plugin.
In this one we will build a synthesizer.
We will create a simple pure sine tone synthesizer with controllable ADSR volume envelope
and gain control. The full code for this example you can find in `examples/sine-synth`
in this repo.

## Installation and project template

As in previous example you can use the same project reverb-csd-juce as
a starting template for your project it has all dependency set up in the cmake file
and it has justfile for convenient build presets.

### Synth JUCE settings

To turn our effect plugin to synthesizer we need to change the params
in the `juce_add_plugin` cmake macros call. Let's look at the typical settings for a synthesizer plugin:

~~~cmake
juce_add_plugin(${PROJECT_NAME}
    # VERSION ...                               # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    COMPANY_NAME CsdArt                          # Specify the name of the plugin's author
    IS_SYNTH TRUE                       # Is this a synth or an effect?
    NEEDS_MIDI_INPUT TRUE               # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE              # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                 # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE    # Does the editor need keyboard focus?
    COPY_PLUGIN_AFTER_BUILD FALSE        # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE CsdA               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE SCsd                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone CLAP                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME ${PROJECT_NAME})        # The name of the final executable, which can differ from the target name
~~~

We expect our plugin to get in MIDI messages and produce single stereo audio output.


## Implementation of the plugin

Our project contains 3 cpp files, one csd file to define audio processing and one template file
to embed the csound code (it was discussed in the reverb example, see Part 1 of the guide):

```
    const.cpp                 // defines constants (parameter names, default values)
    PluginEditor.cpp          // UI definition
    PluginProcessor.cpp       // plugin main entry point and audio processing code
    csd/main.csd              // audio processing with Csound
    embedded_data.h.in        // template file to inline Csound code into the plugin
```

Let's start with main entry point. Let's define the `PluginPorcessor` h and cpp files:
Code for `PluginPorcessor.h`:

```cpp
#pragma once

#include "csd_plugin/audio/Processor.h"
#include "juce_csd/plugin/PluginProcessor.h"
#include "const.h"
#include "EmbeddedCsd.h"

class AudioPluginAudioProcessor final : public juce_csd::PluginProcessor
{
public:
    AudioPluginAudioProcessor(): juce_csd::PluginProcessor(EmbeddedCsd::getMainCsd(), csd_plugin::IOLayout::synth(), init_parameter_spec()){};
    ~AudioPluginAudioProcessor() override {};

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
```

In this code we inherit from the `juce_csd::PluginProcessor` base-class. This class
handles all audio processing and management of plugin control parameters for us.

To initialize the base class instance we provide arguments:

```cpp    AudioPluginAudioProcessor(): juce_csd::PluginProcessor(
      std::string(getEmbeddedCsd()),  // content of Csound file
      csd_plugin::IOLayout::synth(),  // IO-layout
      init_parameter_spec())          // Parameters specification
    {};

```

* Content of Csound code. In our example it's read from the generated `embedded_data.h` file which inlines Csound code.
* Layout for IO-busses. We use predefined layout `IOLayout::synth()` which defines IO-layout typical for synthesizer plugin.
* Parameter specification. It defines control parameters for our plugin.

Implementation of those methods is very short:

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

const juce::String AudioPluginAudioProcessor::getName() const {
    return JucePlugin_Name;
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
```

All heavy lifting of the Csound audio processing loop is done in the internals of the `csound-for-juce` library.
We use JUCE macro `JucePlugin_Name` to read the plugin name from definition in the cmake file.
The UI code is defined in the `AudioPluginAudioProcessirEditor` class. We will discuss the implementation
for this clas in the later sections.


## Audio processing with Csound

Let's creater a csd file which defines audio for ourpure sine synthesizer:

~~~csound
<CsoundSynthesizer>
<CsOptions>

</CsOptions>
<CsInstruments>
sr      =  48000
ksmps   =  32
nchnls  =  2
nchnls_i = 0
0dbfs   =  1.0

massign 0, "main"

chn_k "gain", 1, 2, 0.6, 0, 1
chn_k "attack", 1, 2, 0.02, 0, 1
chn_k "decay", 1, 2, 0.6, 0, 1
chn_k "sustain", 1, 2, 0.6, 0, 1
chn_k "release", 1, 2, 0.6, 0, 1

        instr main
kgain chngetk "gain"
printk2 kgain

iattack chnget "attack"
iattack = iattack + 0.01

idecay chnget "decay"
idecay = idecay + 0.001

isustain chnget "sustain"
isustain = isustain + 0.001

irelease chnget "release"
irelease = irelease + 0.01

iNote notnum
iamp ampmidi 1.0
icps cpsmidinn iNote

kenv madsr iattack, idecay, isustain, irelease
asig oscil3 1, icps, 1
aoutL = asig * iamp * kgain * kenv
aoutR = asig * iamp * kgain * kenv

outs aoutL, aoutR
endin
</CsInstruments>
<CsScore>
f 0 36000
f1 0 128 10 1

e
</CsScore>
</CsoundSynthesizer>
~~~

Let's break it and study in parts.
We set up the global settings:

~~~csound
sr      =  48000  ; sample rate
ksmps   =  32     ; csound audio processing block size
nchnls  =  2      ; stereo audio output
nchnls_i = 0      ; no audio input
0dbfs   =  1.0    ; volume scaling factor
~~~

Next we redirect all midi messages to ther instrument called `main`. This
step is not mandatory. We could rely on Csound defaults and just name
our main instrument with number 1, and Csound would transfer all midid events to it.
It's here just for educational purposes. I'd like to show how we can redirect
midi messages to any instrument (0 - means for all midid channels,
but we can be more specific and set up only specific midi channel. See the docs for massign for
complete reference):

~~~csound
massign 0, "main"
~~~

Next we seup input control channels to control ADSR envelope and total gain for
the synthesizer:

~~~csound
chn_k "gain", 1, 2, 0.6, 0, 1
chn_k "attack", 1, 2, 0.02, 0, 1
chn_k "decay", 1, 2, 0.6, 0, 1
chn_k "sustain", 1, 2, 0.6, 0, 1
chn_k "release", 1, 2, 0.6, 0, 1
~~~

Note that it's important to set second argument to 1, as it's input channel.

Next we define our pure sine instrument. It's important to use i-rate
variant to read ADSR constants. So we use `chngetk` for gain control
it can change over the playing of a single note. And we use `chnget` which
runs at i-rate as `madsr` opcode which forms the ADSR envelope expects
it's arguments to be defined at i-rate. If we use `chngetk` - for ADSR comnstant
we will get 0 values or values from the previous note settings for the values
of our ADSR control variables. We add some tiny numbers to avoid zero values for
envelopes. For some values it's not dezirable to set them to zero otherwise
we will hear pops and clicks in the produced audio.

~~~csound
        instr main
kgain chngetk "gain"
printk2 kgain

iattack chnget "attack"
iattack = iattack + 0.01

idecay chnget "decay"
idecay = idecay + 0.001

isustain chnget "sustain"
isustain = isustain + 0.001

irelease chnget "release"
irelease = irelease + 0.01

iNote notnum
iamp ampmidi 1.0
icps cpsmidinn iNote

kenv madsr iattack, idecay, isustain, irelease
asig oscil3 1, icps, 1
aoutL = asig * iamp * kgain * kenv
aoutR = asig * iamp * kgain * kenv

outs aoutL, aoutR
endin
~~~

At the final section we ask csound to keep alive forever. And create
pure sine wave ftable to play it in our synthesizer:

~~~csound
</CsInstruments>
<CsScore>
f 0 36000
f1 0 128 10 1

e
</CsScore>
</CsoundSynthesizer>
~~~

With this code we have defined audio processor for our synthesizer plugin
and created control channels over which we can communicate parameter
changes from UI / DAW to Csound process.

## IO-layout

IO-layout specifies how many inputs and outputs plugin needs and weather it processes MIDI messages
or side-chain inputs. You can study the docs for complete list of members. So far we can use
predefined constructor: `synth`:

```cpp
  IOLayout::synth()
```

Which creates layout with no audio input and single stereo output, which expects input MIDI events
and has no side-chain inputs.

### Parameters for the plugin

Let's look at the specification of the parameters for our plugin:

Parameters are defined in `const` module. In the header file `const.h` we define the names for the parameters,
 note that ID's of the parameters should be identical to the names of Csound control channels:

```cpp
#pragma once
#include <string>
#include "juce_csd/params/Parameters.h"

using namespace juce_csd;

namespace names {

static const std::string attack("attack");
static const std::string decay("decay");
static const std::string sustain("sustain");
static const std::string release("release");
static const std::string gain("gain");
static const std::string window_width("window_width");
static const std::string window_height("window_height");

}

namespace values {

static const int window_height = 200;
static const int window_width = 620;

}

ParameterSpec init_parameter_spec();
```

Lett's define the parameters in the `const.cpp`. For a detailed review on
how to setup parameters see the Part 1 of the tutorial. The setup is similiar
to the reverb case. We are going to create 5 knobs which will control ADSR envelope and gain:

~~~cpp
#include <string>
#include <juce_csd/params/Parameters.h>
#include "const.h"

using namespace juce_csd;

ParameterSpec init_parameter_spec() {
    std::vector<AudioParameterFloatSpec> audio_floats_spec =
            { AudioParameterFloatSpec(names::attack, names::attack, 0.0, 1.0, 0.01, 0.1 ),
             AudioParameterFloatSpec(names::decay, names::decay, 0.0, 1.0, 0.01, 0.5 ),
             AudioParameterFloatSpec(names::sustain, names::sustain, 0.0, 1.0, 0.01, 0.5 ),
             AudioParameterFloatSpec(names::release, names::release, 0.0, 1.0, 0.01, 0.1 ),
             AudioParameterFloatSpec(names::gain, names::gain, 0.0, 1.0, 0.01, 0.8 )};

    std::vector<UiParameterSpec> ui_spec =
            { UiParameterSpec(names::window_height, values::window_height),
              UiParameterSpec(names::window_width, values::window_width)
            };
    return ParameterSpec
        { .audio_floats = audio_floats_spec
        , .ui = ui_spec
        };
}
~~~

Also besides the Csound control parameters we have defined the
UI-parameters for the current window size. They have the same meaning
as in reverb example. We declare it in `ParameterSpec` so that
window size values could be saved between UI launches and DAw could recall the
last window size for our plugin which was set up by the user.

## UI code for the plugin

in the UI code for the synthesizer we define the class for Editor which
holds 5 knobs. Four for ADSR envelope controls and one for gain.

~~~cpp
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
~~~

Also we define helper methods for parameter attachments which
link UI controls with Csound parameters and some members for UI-related elements:
labels, fonts, window size constraints.

Here is the implementation of the UI.The code is basically
the same as for the reverb example. We initialize knobs for parameters
and attach them to the corresponding csound parameters.

Note that it's important to call `addAndMakeVisisble` on JUCE component
for it to be visisble ion the plugin window:

~~~cpp
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
~~~

## Build the plugin

We can build the project with commands (see also justfile for reverb example in the repo
for up to date commands):

Prepare build files. It will create the `build` directory with all the build files:

```
cmake -B build -G Ninja
```

Build the project:

```
cmake --build build
```

I use the justfile to automate this task:

```
  build:
    cmake -B build -G Ninja
    cmake --build build
```

also you can put here the code to launch standalone app for testing
or copy the generated plugin to your global folder dedicated to plugins on your PC.

And do:

```
  just build
```

Artefacts are generated inside the build directory:

```
build/src/SineCsdExample_artefacts/
```

to test the plugin in the DAW, copy the plugin from the artefacts directory
to the directory where your DAW watches for the plugins.

That's it completes our part 2 of the guide.
