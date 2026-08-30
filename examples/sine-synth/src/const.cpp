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

