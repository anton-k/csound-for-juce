#include <catch2/catch_test_macros.hpp>
#include "juce_csd/params/Parameters.h"
#include "juce_csd/params/JsonSerializer.hpp"
#include <juce_core/juce_core.h>

using namespace juce_csd;

TEST_CASE("JsonSerializer: Handles empty and malformed JSON gracefully", "[Serialization]") {
    ParameterSpec spec;
    spec.version = 1;
    ParameterSpecMap spec_map(spec);

    AudioParameterList audio_params;
    UiParameterList ui_params;

    SECTION("Empty input fails safely") {
        juce::MemoryBlock mb;
        juce::MemoryInputStream input(mb, false);

        auto result = JsonSerializer::deserialize(spec_map, input, audio_params, ui_params);
        REQUIRE(result.failed());
    }

    SECTION("Malformed JSON fails safely") {
        juce::String bad_json = "{ \"version\": 1, \"audio\": { broken json }";
        juce::MemoryBlock mb(bad_json.toRawUTF8(), bad_json.getNumBytesAsUTF8());
        juce::MemoryInputStream input(mb, false);

        auto result = JsonSerializer::deserialize(spec_map, input, audio_params, ui_params);
        REQUIRE(result.failed());
    }
}

