#pragma once
#include <string>
#include "juce_csd/params/Parameters.h"

using namespace juce_csd;

namespace names {

static const std::string attack("attack");
static const std::string decay("decay");
static const std::string sustain("sustain");
static const std::string release("release");
static const std::string gain("gain");
static const std::string window_width("window_width");
static const std::string window_height("window_height");

}

namespace values {

static const int window_height = 200;
static const int window_width = 620;

}

ParameterSpec init_parameter_spec();
