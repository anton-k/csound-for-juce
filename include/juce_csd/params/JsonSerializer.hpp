#pragma once

#include "Parameters.h"
#include "juce_core/juce_core.h"

namespace juce_csd {

class JsonSerializer {
  public:
    static void serialize(const AudioParameterFloatList&, const AudioParameterBoolList&, const AudioParameterChoiceList&, const UiParameterList&, juce::OutputStream&);
    static juce::Result deserialize(juce::InputStream&, AudioParameterFloatList&, AudioParameterBoolList&, AudioParameterChoiceList&, UiParameterList&);
};

}
