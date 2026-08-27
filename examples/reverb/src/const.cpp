#include <string>
#include <juce_csd/params/Parameters.h>
#include "const.h"

using namespace juce_csd;

ParameterSpec init_parameter_spec() {
    std::vector<AudioParameterFloatSpec> audio_floats_spec =
            { AudioParameterFloatSpec(names::size, names::size, 0.0, 1.0, 0.01, 0.3 ),
             AudioParameterFloatSpec(names::tone, names::tone, 0.0, 1.0, 0.01, 1.0 ),
             AudioParameterFloatSpec(names::mix, names::mix, 0.0, 1.0, 0.01, 0.12 )};

    std::vector<UiParameterSpec> ui_spec =
            { UiParameterSpec(names::window_height, 200.0),
              UiParameterSpec(names::window_width, 400.0)
            };
    return ParameterSpec
        { .audio_floats = audio_floats_spec
        , .ui = ui_spec
        };
}

