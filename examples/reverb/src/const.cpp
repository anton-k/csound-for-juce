#include <string>
#include <juce_csd/params/Parameters.h>
#include "const.h"

using namespace juce_csd;

/// Defines specification of the parameters for the plugin.
// Reverb plugin has 3 audio float parameters which define
// size of the reverb, how much frequencies of reflections are not dumped, and mix for dry/wet ratio.
// Single choice parameter controlls reverb type.
ParameterSpec init_parameter_spec() {
    std::vector<AudioParameterFloatSpec> audio_floats_spec =
            { AudioParameterFloatSpec(names::size, names::size, 0.0, 1.0, 0.01, 0.3 ),
             AudioParameterFloatSpec(names::tone, names::tone, 0.0, 1.0, 0.01, 1.0 ),
             AudioParameterFloatSpec(names::mix, names::mix, 0.0, 1.0, 0.01, 0.12 )};

    std::vector<AudioParameterChoiceSpec> audio_choices_spec =
            { AudioParameterChoiceSpec(names::reverb_type, "Reverb type", {"Sean Costello", "Freeverb", "Nverb"}, 1) };

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

