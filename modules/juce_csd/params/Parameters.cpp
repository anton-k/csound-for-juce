#include "Parameters.h"
#include "juce_audio_processors_headless/juce_audio_processors_headless.h"
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "JsonSerializer.hpp"

namespace juce_csd {

Parameters::Parameters(juce::AudioProcessor& processor, const ParameterSpec& spec):
    audio_parameters(), ui_parameters(), sensor_parameters() {
    init_audio_parameters(processor, spec.audio);
    init_ui_parameters(spec.ui);
    init_sensor_parameters(spec.sensor);
}

juce::AudioParameterFloat& Parameters::get_audio_parameter_ref(const std::string& name) {
  return *audio_parameters.at(name);
}

void Parameters::init_audio_parameters(juce::AudioProcessor& processor, const std::vector<AudioParameterSpec>& param_specs) {
    for (const auto& spec : param_specs) {
        DBG("Creating parameter: " << spec.name);

        auto* param = new juce::AudioParameterFloat(
            spec.id,  // parameterID
            spec.name,  // parameterName
            spec.min,  // minValue
            spec.max,  // maxValue
            spec.default_value   // defaultValue
        );

        // Add to processor
        processor.addParameter(param);

        // Store pointer
        audio_parameters.insert(std::pair(spec.id, param));
    }
}

void Parameters::init_ui_parameters(const std::vector<UiParameterSpec>& parameter_specs) {
    for (const UiParameterSpec& spec : parameter_specs) {
        ui_parameters.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(spec.id),
            std::forward_as_tuple(spec.default_value));
    }
}

void Parameters::init_sensor_parameters(const std::vector<SensorParameterSpec>& parameter_specs) {
    for (const SensorParameterSpec& spec : parameter_specs) {
        sensor_parameters.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(spec.id),
            std::forward_as_tuple(spec.default_value));
    }
}

void Parameters::update_on_process(Csound* csound) {
  for (auto& param: audio_parameters) {
    csound->SetControlChannel(param.first.c_str(), param.second->get());
  }
  for (auto& param: sensor_parameters) {
    param.second.store(csound->GetControlChannel(param.first.c_str()));
  }
}

void Parameters::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream os{destData, true};
    JsonSerializer::serialize(audio_parameters, ui_parameters, os);
}

void Parameters::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream is{data, static_cast<size_t>(sizeInBytes), false};
    const juce::Result result = JsonSerializer::deserialize(is, audio_parameters, ui_parameters);
    if (result.failed()) {
        DBG(result.getErrorMessage());
    }
    // TODO: smoothe parameter transition
}

void Parameters::set_ui_parameter(const std::string& id, float value) {
  auto it = ui_parameters.find(id);
  if (it != ui_parameters.end()) {
    it->second.store(value);
  }
}

std::optional<float> Parameters::get_ui_parameter(const std::string& id) {
  auto it = ui_parameters.find(id);
  if (it == ui_parameters.end()) {
    return std::optional<float>();
  } else {
    return std::optional(it->second.load());
  }
}

std::optional<float> Parameters::get_sensor_parameter(const std::string& id) {
  auto it = sensor_parameters.find(id);
  if (it == sensor_parameters.end()) {
    return std::optional<float>();
  } else {
    return std::optional(it->second.load());
  }
}

}
