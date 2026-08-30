#pragma once
#include <string>
#include "juce_csd/params/Parameters.h"

using namespace juce_csd;

/// Defines all names for plugin parameters
namespace names {

static const std::string size("size");
static const std::string tone("tone");
static const std::string mix("mix");
static const std::string reverb_type("reverb_type");
static const std::string window_width("window_width");
static const std::string window_height("window_height");

}

/// Defines default values for the parameters
namespace values {

static const int window_height = 250;
static const int window_width = 400;

}

ParameterSpec init_parameter_spec();
