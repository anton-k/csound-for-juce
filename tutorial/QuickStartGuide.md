# Quick start guide

The library csound-for-juce provides classes to create audio plugins with JUCE.
The audio processing and parameter managment is covered with the library and done with Csound over API.
The user needs to write only UI code.

There are two classes suitable for different style of usage `Processor` and `PluginProcessor`.
The class `Porcessor` is for compopsition. We can create a value of the `Processor`
and store it as a private member of our plugin class.
The class `PluginProcessor` is for usage with inheritance. We inherit from the `PluginProcessor`
class which implements almost all methods of JUCE base class `AudioPorcessor`.
The user needs to provide only 3 methods to complete the plugin:

```cpp
    // Create UI
    juce::AudioProcessorEditor* createEditor() override;

    // Tell to the Host that plugin has UI
    bool hasEditor() const override;

    // Return the name of the plugin
    const juce::String getName() const override;
```

The simplest way to define a plugin is to use the `PluginProcessor` class.
In the library repo there is a directory [`examples`](https://github.com/anton-k/csound-for-juce/tree/main/examples) which contains examples of the plugins.
In this guide we will study the reverb example from the repo [`reverb-csd-juce`](https://github.com/anton-k/reverb-csd-juce)
as an example on how to build plugins with `csound-for-juce` library.

### How to use the library in your project

The library csound-for-juce can be included as a dependency for our project with `cmake` and `CPM manager`.
We can use the repo [`reverb-csd-juce`](https://github.com/anton-k/reverb-csd-juce) as an example
on how to initialize the basic project that uses libraries `csound-for-juce` and the `JUCE`.

We can create the repo that contains directory `src` for our sources.
At the top level we will define the root `CMakeLists.txt` file:

```
cmake_minimum_required(VERSION 3.24)
project(MyPluginName VERSION 0.0.1)
add_subdirectory(src)
```

At the `src` we define the `CMakeLists.txt` file with definition of the dependencies and
description of our plugin for the JUCE:

```
include(../cmake/CPM.cmake)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
CPMAddPackage("gh:juce-framework/JUCE#3af3ce009f6a02f6fa651008fffb5b41743a9fab")
CPMAddPackage("gh:anton-k/csound-for-juce#d5c803e4dc06164ec718a2d01c29ee054829260a")

juce_add_plugin(${PROJECT_NAME}
    # VERSION ...                        # Set this if the plugin version is different to the project version
    COMPANY_NAME CsdArt                  # Specify the name of the plugin's author
    IS_SYNTH FALSE                       # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE               # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE              # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                 # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE    # Does the editor need keyboard focus?
    # COPY_PLUGIN_AFTER_BUILD TRUE       # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE CsdA        # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Rev0                     # A unique four-character plugin id with exactly one upper-case character
                                         # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone CLAP      # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME ${PROJECT_NAME})        # The name of the final executable, which can differ from the target name

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_sources(${PROJECT_NAME}
    PRIVATE
        const.cpp
        PluginEditor.cpp
        PluginProcessor.cpp
    )

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Read the file content
file(READ "csd/reverb.csd" EMBEDDED_CONTENT)

# Configure the header file (replace @EMBEDDED_CONTENT@)
configure_file(
    "embedded_data.h.in"
    "embedded_data.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/csd/reverb.csd"
    "${CMAKE_CURRENT_BINARY_DIR}/csd/reverb.csd"
    COPYONLY
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/csd
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce_csd

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
```

This is a typical template for the plugin.
In this file we include csound-for-juce as external dependency:

```
CPMAddPackage("gh:juce-framework/JUCE#3af3ce009f6a02f6fa651008fffb5b41743a9fab")
CPMAddPackage("gh:anton-k/csound-for-juce#d5c803e4dc06164ec718a2d01c29ee054829260a")
```

Next we define the plugin description. We specify all the constants that JUCE needs to create a plugin:

```
juce_add_plugin(${PROJECT_NAME}
    # VERSION ...                        # Set this if the plugin version is different to the project version
    COMPANY_NAME CsdArt                  # Specify the name of the plugin's author
    IS_SYNTH FALSE                       # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE               # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE              # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                 # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE    # Does the editor need keyboard focus?
    # COPY_PLUGIN_AFTER_BUILD TRUE       # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE CsdA        # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Rev0                     # A unique four-character plugin id with exactly one upper-case character
                                         # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone CLAP      # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME ${PROJECT_NAME})        # The name of the final executable, which can differ from the target name
```

This is a typical definition of the effect plugin.
Then we specify the cpp language standard. and define some JUCE related constants.

The following code embedds csd file into cpp code. We have a directory `src/csd` which
conatains a file reverb.csd with definition of audio processing in Csound code for our plugin.
This CMake code allows us to embed this file into cpp code:

```
# Read the file content
file(READ "csd/reverb.csd" EMBEDDED_CONTENT)

# Configure the header file (replace @EMBEDDED_CONTENT@)
configure_file(
    "embedded_data.h.in"
    "embedded_data.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/csd/reverb.csd"
    "${CMAKE_CURRENT_BINARY_DIR}/csd/reverb.csd"
    COPYONLY
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/csd
    ${CMAKE_CURRENT_BINARY_DIR}
)
```

We have template file `embedded_data.h.in` that is used to embed Csound code.
Here is the content of the file:

```
#pragma once

#include <string>
#include <string_view>

inline std::string getEmbeddedCsd() {
    static const std::string data(R"(@EMBEDDED_CONTENT@)");
    return data;
}
```

Cmake will create a header file `embedded_data.h` which is based on this template. Only
`EMBEDDED_CONTENT` will be replaced with csound code.

Next we define our dependencies:

```
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce_csd

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
```

## Implementation of the plugin

Our project contains 3 cpp files, one csd file to define audio processing and one template file
to embed the csound code (it was discussed in the previous section):

```
    const.cpp                 // defines constants (parameter names, default values)
    PluginEditor.cpp          // UI definition
    PluginProcessor.cpp       // plugin main entry point and audio processing code
    csd/reverb.csd            // audio processing with Csound
    embedded_data.h.in        // template file to inline Csound code into the plugin
```

Let's start with main entry point. Let's define the Porcessor h and cpp files:
Code for `Processing.h`:

```cpp
#pragma once

#include <csd_plugin/audio/Processor.h>
#include <juce_csd/plugin/PluginProcessor.h>
#include "const.h"
#include "embedded_data.h"

class AudioPluginAudioProcessor final : public juce_csd::PluginProcessor
{
public:
    AudioPluginAudioProcessor(): juce_csd::PluginProcessor(std::string(getEmbeddedCsd()), csd_plugin::IOLayout::fx(), init_parameter_spec()){};
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
      csd_plugin::IOLayout::fx(),     // IO-layout
      init_parameter_spec())          // Parameters specification
    {};

```

* Content of Csound code. In our example it's read from the generated `embedded_data.h` file which inlines Csound code.
* Layout for IO-busses. We use predefined layout `IOLayout::fx()` which defines IO-layout typical for effect plugins.
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

### Csound audio processing

The library handles Csound audio processing for us. We need to define
the Csound code with proper amounts of input/output audio channels and
use correct names for control channels to receive the updates of the parameters
form the plugin UI or host.

Let's define the code for the reverb plugin. Our pluggin has 3 controls for
the `size` of the room, `tone` of reflection (how much low-pass filter is applied to the reflected signal)
and `mix` for dry/wet ratio:

```csound
<CsoundSynthesizer>
<CsOptions>

</CsOptions>
<CsInstruments>
sr      =  48000
ksmps   =  4
nchnls  =  2
nchnls_i = 2
0dbfs   =  1.0

chn_k "size", 1, 2, 0.6, 0, 1
chnset 0.6, "size"

chn_k "tone", 1, 2, 0.6, 0, 1
chnset 0.6, "tone"

chn_k "mix", 1, 2, 0.6, 0, 1
chnset 0.6, "mix"

chn_k "reverb_type", 1, 1, 1, 1, 2
chnset 0.6, "reverb_type"

        instr 1
kfeedback chngetk "size"
kcutOff chngetk "tone"
kmix chngetk "mix"
printk2 kfeedback
printk2 kcutOff
printk2 kmix

ainL, ainR  inch 1, 2
awetL, awetR  reverbsc ainL, ainR, kfeedback, 20000 * kcutOff
aoutL = (1 - kmix) * ainL + kmix * (2 * awetL)
aoutR = (1 - kmix) * ainR + kmix * (2 * awetR)
outs aoutL, aoutR
endin
</CsInstruments>
<CsScore>
f 0 36000
i 1 0 -1
e
</CsScore>
</CsoundSynthesizer>
```

Let's study this code.
Plugin definition starts with global settings for the audio processing. `sr` stands for sample rate:

```csound
sr = 48000
```

Note that this setting will be reset to the sample rate that is requested by the host
on preparation stage of the plugin.

We define the `ksmps` which processes audio in blocks of the `ksmps` size inside the Csound engine:

```csound
ksmps = 4
```

The `ksmps` affects latency of the effect plugin. For an effect plugin latency can be no smaller
than `ksmps` value. For a plugin which process no inputs we can achieve zero latency.
There is a trade off for the `ksmps` value. Lower `ksmps` means less latency for the plugin (it's more responsive)
but higheer `ksmps` means less CPU-usage during audio processing. Note that MIDI-events
are processed once per `ksmps` block.

We create an effect plugin so we need to process stereo input and produce stereo output:

```csound
nchnls  =  2  ; number of output channels
nchnls_i = 2  ; number of input channels
```

Also we define scaling factor for audio amplitudes:

```csound
0dbfs   =  1.0
```

Next we define paramneters for our plugin. We communicate with cpp JUCE code over Csound API with
control channels. We define control channels for all parameters. Note that parameter id should
be the same as the name of the control channel in the Csound code:

```csound
chn_k "size", 1, 2, 0.6, 0, 1
chnset 0.6, "size"

chn_k "tone", 1, 2, 0.6, 0, 1
chnset 0.6, "tone"

chn_k "mix", 1, 2, 0.6, 0, 1
chnset 0.6, "mix"
```

See the docs on [`chn_k`](https://csound.com/docs/manual/chn.html) opcode on how to define control channels.

We are ready to define the reverb audio processing:

```csound
          instr 1
kfeedback chngetk "size"
kcutOff chngetk "tone"
kmix chngetk "mix"

ainL, ainR  inch 1, 2
awetL, awetR  reverbsc ainL, ainR, kfeedback, 20000 * kcutOff
aoutL = (1 - kmix) * ainL + kmix * (2 * awetL)
aoutR = (1 - kmix) * ainR + kmix * (2 * awetR)
outs aoutL, aoutR
endin
```

We use `reverbsc` opcode to process audio. It's a solid and beautiful reverb algorithm.
The audio input is read with the `inch` opcode and output produced with the `outs` opcode.

In the score section we need to turn on the instrument 1 which
performs the audio processing:

```csound
<CsScore>
f 0 36000
i 1 0 -1
e
</CsScore>
```

Those two lines make Csound work forever and ensure that reverb processor instrument is
always on.

### IO-layout

IO-layout specifies how many inputs and outputs plugin needs and weather it processes MIDI messages
or side-chain inputs. You can study the docs for complete list of members. So far we can use
predefined constructor: `fx`:

```cpp
  IOLayout::fx()
```

Which creates layout with single stereo input and single stereo output, which deos not work with MIDI
and has no side-chain inputs.

### Parameters for the plugin

Let's look at the specification of the parameters for our plugin:

Parameters are defined in `const` module:

In the header file `const.h` we define the names for the parameters, note that ID's of the parameters should be
identical to the names of Csound control channels:

```cpp
#pragma once
#include <string>
#include "juce_csd/params/Parameters.h"

using namespace juce_csd;

namespace names {

static const std::string size("size");
static const std::string tone("tone");
static const std::string mix("mix");
static const std::string reverb_type("reverb_type");
static const std::string window_width("window_width");
static const std::string window_height("window_height");

}

namespace values {

static const int window_height = 250;
static const int window_width = 400;

}

ParameterSpec init_parameter_spec();
```

Lett's define the parameters in the `const.cpp`:

```cpp
#include <string>
#include <juce_csd/params/Parameters.h>
#include "const.h"

using namespace juce_csd;

ParameterSpec init_parameter_spec() {
    std::vector<AudioParameterFloatSpec> audio_floats_spec =
            { AudioParameterFloatSpec(names::size, names::size, 0.0, 1.0, 0.01, 0.3 ),
             AudioParameterFloatSpec(names::tone, names::tone, 0.0, 1.0, 0.01, 1.0 ),
             AudioParameterFloatSpec(names::mix, names::mix, 0.0, 1.0, 0.01, 0.12 )};

    std::vector<AudioParameterChoiceSpec> audio_choices_spec =
            { AudioParameterChoiceSpec(names::reverb_type, "Reverb type", {"Sean Constello", "Freeverb", "Nverb"}, 1) };

    std::vector<UiParameterSpec> ui_spec =
            { UiParameterSpec(names::window_height, values::window_height),
              UiParameterSpec(names::window_width, values::window_width)
            };
    return ParameterSpec
        { .audio_floats = audio_floats_spec
        , .audio_choices = audio_choices_spec
        , .ui = ui_spec
        };
}
```

We have several groups of the parameters.

* audio parameters (control Csound control channels over UI or host)

   * floats - controlled by continuous UI widgets (sliders and knobs)
   * booleans - controlled by buttons and switches
   * integers - discrete integer input
   * choices - set of predefined modes (controlled by drop-down lists or radio butons)

* UI parameters (specify some UI state that should be persisted)

* sensors - floats that are read from Csound control channels. Can be useful to
   measure some audio features (like volume, LR-balance, crest factor and so on)

* host parameters - values which are read from host (BPM, play-head position in the DAW, tempo etc.)

Audio parameters and UI parameters are persisted in the plugin preset or between launches of the plougin UI window.

For our plugin we have 3 float parameters (size, tone and mix), 1 choice parameter (type of the reverb),
and two UI parameters which psecify window size of the plugin.

Audio parameter specification follows the arguments for the float audio parameter from the JUCE:

```cpp
  AudioParameterFloatSpec(names::size, names::size, 0.0, 1.0, 0.01, 0.3 )
```

It expects: parameter ID, parameter name, minimum and maximum values, step of change and default value.
Note that be default all continuous float parameters are smoothed to eliminate zipper noise (so we don't need
to do it in the Csound code with `portk` opcode). If you want to turn off the smoothing effect you
can specify float parameter type as `Discrete`, by default it's set to `Continuous`. See the docs in
library source code on how to do that.

An example on how to define choice parameter:

```cpp
  AudioParameterChoiceSpec(names::reverb_type, "Reverb type", {"Sean Constello", "Freeverb", "Nverb"}, 1) }
```
In our plugin we can use choice parameter to change the mode of the reverb algorithm.
So far it's not used in the Csound code. But it serves as an example on how it can be defined in the Cpp code.
Note that values for choice are 1-based integers for options. So the first mode in the Csound will be set to 1 and the last
one `Nverb` is set to 3. It follows the JUCE convention where zero value is reserved as "nothing is set" value.

Our plugin does not use sensor and host parameters and by default they are set to an empty vector.

The parameters tell to the cpp audio processing code which parameters the underlying Csound code supports.
The audio parameters are inputs for Csound (the UI sets those parameters to Csound),
and sensors are outputs which we read from Csound to UI.
