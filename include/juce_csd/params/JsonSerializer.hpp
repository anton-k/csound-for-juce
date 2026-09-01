#pragma once

#include "Parameters.h"
#include "juce_core/juce_core.h"

namespace juce_csd {

/**
 * @class JsonSerializer
 * @brief Defines serialization for the application parameters (state).
 *
 * The Plugin's state is converted to/from JSON for persistance.
 */
class JsonSerializer {
  public:
    static void serialize(const ParameterSpecMap& spec, const AudioParameterList&, const UiParameterList&, juce::OutputStream&);
    static juce::Result deserialize(const ParameterSpecMap& spec, juce::InputStream&, AudioParameterList&, UiParameterList&);
};

}
