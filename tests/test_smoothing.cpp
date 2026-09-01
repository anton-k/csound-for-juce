
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "juce_csd/params/Parameters.h"

using namespace juce_csd;
using Catch::Approx;

TEST_CASE("SmoothedParam: Continuous smoothing reaches target exactly", "[Parameters]") {
    // We pass nullptr for the JUCE parameter because the math methods don't dereference it
    SmoothedParam param(nullptr, ParameterType::Continuous, 0.0f, 10.0f);
    param.set_sample_rate(48000.0f); // 480 samples for 10ms

    param.set_target(1.0f);
    REQUIRE(param.samples_remaining == 480);

    float current = 0.0f;
    // Process in blocks of 128
    current = param.process(128);
    REQUIRE(param.samples_remaining == 352);

    current = param.process(128);
    REQUIRE(param.samples_remaining == 224);

    current = param.process(128);
    REQUIRE(param.samples_remaining == 96);

    // Final block: should snap to exact target and set remaining to 0
    current = param.process(128);
    REQUIRE(param.samples_remaining == 0);
    REQUIRE(current == Approx(1.0f));
}

TEST_CASE("SmoothedParam: Discrete parameter snaps instantly", "[Parameters]") {
    SmoothedParam param(nullptr, ParameterType::Discrete, 0.0f, 10.0f);
    param.set_sample_rate(48000.0f);

    param.set_target(1.0f);
    REQUIRE(param.samples_remaining == 0);

    float current = param.process(128);
    REQUIRE(current == Approx(1.0f));
}

TEST_CASE("SmoothedParam: force_instant bypasses smoothing", "[Parameters]") {
    SmoothedParam param(nullptr, ParameterType::Continuous, 0.0f, 10.0f);
    param.set_sample_rate(48000.0f);

    param.set_target(1.0f, true); // force_instant = true
    REQUIRE(param.samples_remaining == 0);
    REQUIRE(param.current_value == Approx(1.0f));
}

