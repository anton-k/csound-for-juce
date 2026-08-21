#pragma once
#include <string>
#include "juce_csd/params/Parameters.h"

using namespace juce_csd;

namespace names {

static const std::string size("size");
static const std::string tone("tone");
static const std::string mix("mix");
static const std::string window_width("window_width");
static const std::string window_height("window_height");

}

ParameterSpec init_parameter_spec();
