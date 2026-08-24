#include <juce_csd/params/Parameters.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <juce_csd/params/JsonSerializer.hpp>

namespace juce_csd {

Parameters::Parameters(juce::AudioProcessor& processor, const ParameterSpec& spec):
    audio_parameters(), ui_parameters(), sensor_parameters() {
    init_audio_parameters(processor, spec.audio);
    init_ui_parameters(spec.ui);
    init_sensor_parameters(spec.sensor);
}

juce::AudioParameterFloat& Parameters::get_audio_parameter_ref(const std::string& id) {
    auto it = audio_parameters.find(id);
    if (it != audio_parameters.end() && it->second.param != nullptr) {
        return *(it->second.param);
    }
    throw std::runtime_error("Parameter not found: " + id);
}

void Parameters::init_audio_parameters(juce::AudioProcessor& processor, const std::vector<AudioParameterSpec>& param_specs) {
    for (const auto& spec : param_specs) {
        DBG("Creating parameter: " << spec.name);

        juce::NormalisableRange<float> range(spec.min, spec.max, spec.step);
        auto* param = new juce::AudioParameterFloat(
                spec.id, spec.name, range, spec.default_value);

        // Add to processor
        processor.addParameter(param);

        // Store pointer
        audio_parameters.emplace(spec.id, SmoothedParam(param, spec.type, spec.default_value, spec.smoothing_time_ms));


    }
}

void Parameters::init_ui_parameters(const std::vector<UiParameterSpec>& parameter_specs) {
    for (const UiParameterSpec& spec : parameter_specs) {
      ui_parameters.emplace(spec.id, spec.default_value);
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

void Parameters::prepare(double sample_rate, int max_block_size) {
    for (auto& param : audio_parameters) {
        param.second.set_sample_rate(static_cast<float>(sample_rate));
    }
}

void Parameters::update_on_process(Csound* csound) {
  for (auto& param: audio_parameters) {
    auto& sp = param.second;
    if (sp.param != nullptr) {
      float new_target = sp.param->get();
      sp.set_target(new_target);
      float value_to_send = sp.process();
      csound->SetControlChannel(param.first.c_str(), static_cast<double>(value_to_send));
    }
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
    // Note: Smoothing is intentionally bypassed during state restoration
    // via the force_instant = true flag in the deserializer, preventing weird ramping.
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
