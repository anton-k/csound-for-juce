#include <string>
#include <juce_csd/params/Parameters.h>
#include "const.h"

using namespace juce_csd;

ParameterSpec init_parameter_spec() {
    std::vector<AudioParameterSpec> audio_spec =
            { AudioParameterSpec(names::size, names::size, 0.0, 1.0, 0.01, 0.3 ),
             AudioParameterSpec(names::tone, names::tone, 0.0, 1.0, 0.01, 1.0 ),
             AudioParameterSpec(names::mix, names::mix, 0.0, 1.0, 0.01, 0.12 )};
    std::vector<UiParameterSpec> ui_spec =
            { UiParameterSpec(names::window_height, 200.0),
              UiParameterSpec(names::window_width, 400.0)
            };
    std::vector<SensorParameterSpec> sensor_spec = {};
    return ParameterSpec(audio_spec, ui_spec, sensor_spec);
}

