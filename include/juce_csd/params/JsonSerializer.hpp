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
    static void serialize(const AudioParameterList&, const UiParameterList&, juce::OutputStream&);
    static juce::Result deserialize(juce::InputStream&, AudioParameterList&, UiParameterList&);
};

}
