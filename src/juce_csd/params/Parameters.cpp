#include <juce_audio_basics/juce_audio_basics.h>
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
    float_audio_parameters(), bool_audio_parameters(), ui_parameters(), sensor_parameters() {
    init_float_audio_parameters(processor, spec.audio_floats);
    init_bool_audio_parameters(processor, spec.audio_bools);
    init_ui_parameters(spec.ui);
    init_sensor_parameters(spec.sensor);
}

juce::AudioParameterFloat& Parameters::get_float_audio_parameter_ref(const std::string& id) {
    auto it = float_audio_parameters.find(id);
    if (it != float_audio_parameters.end() && it->second.param != nullptr) {
        return *(it->second.param);
    }
    throw std::runtime_error("Parameter not found: " + id);
}

juce::AudioParameterBool& Parameters::get_bool_audio_parameter_ref(const std::string& id) {
    auto it = bool_audio_parameters.find(id);
    if (it != bool_audio_parameters.end() && it->second != nullptr) {
        return *(it->second);
    }
    throw std::runtime_error("Parameter not found: " + id);
}


void Parameters::init_float_audio_parameters(juce::AudioProcessor& processor, const std::vector<AudioParameterFloatSpec>& param_specs) {
    for (const auto& spec : param_specs) {
        DBG("Creating float parameter: " << spec.name);
        juce::NormalisableRange<float> range(spec.min, spec.max, spec.step);
        auto* param = new juce::AudioParameterFloat(
                spec.id, spec.name, range, spec.default_value);

        // Add to processor
        processor.addParameter(param);

        // Store pointer
        float_audio_parameters.emplace(spec.id, SmoothedParam(param, spec.type, spec.default_value, spec.smoothing_time_ms));
    }
}

void Parameters::init_bool_audio_parameters(juce::AudioProcessor& processor, const std::vector<AudioParameterBoolSpec>& param_specs) {
    for (const auto& spec : param_specs) {
        DBG("Creating boolean parameter: " << spec.name);
        auto* param = new juce::AudioParameterBool(spec.id, spec.name, spec.default_value);

        // Add to processor
        processor.addParameter(param);

        // Store pointer
        bool_audio_parameters.emplace(spec.id, param);
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
    for (auto& param : float_audio_parameters) {
        param.second.set_sample_rate(static_cast<float>(sample_rate));
    }
}

void Parameters::update_on_process(Csound* csound, int block_size, juce::AudioPlayHead* play_head) {
  update_float_audio_params(csound, block_size);
  update_bool_audio_params(csound);
  update_sensor_params(csound);
  update_host_params(csound, play_head);
}

void Parameters::update_float_audio_params(Csound* csound, int block_size) {
  for (auto& param: float_audio_parameters) {
    auto& sp = param.second;
    if (sp.param != nullptr) {
      float new_target = sp.param->get();
      sp.set_target(new_target);
      float value_to_send = sp.process(block_size);
      csound->SetControlChannel(param.first.c_str(), static_cast<double>(value_to_send));
    }
  }
}

void Parameters::update_bool_audio_params(Csound* csound) {
  for (auto& param: bool_audio_parameters) {
    if (param.second != nullptr) {
      csound->SetControlChannel(param.first.c_str(), (param.second->get()? 1.0: 0.0));
    }
  }
}

void Parameters::update_sensor_params(Csound* csound) {
  for (auto& param: sensor_parameters) {
    param.second.store(csound->GetControlChannel(param.first.c_str()));
  }
}

namespace {
  void set_optional_csound_param(Csound* csound, const std::string& name, const juce::Optional<double>& value) {
     if (value.hasValue()) {
       csound->SetControlChannel(name.c_str(), *value);
     }
  }
}

void Parameters::update_host_params(Csound* csound, juce::AudioPlayHead* play_head) {
  if (host_parameters.size() > 0 && play_head != nullptr) {
     auto pos = play_head->getPosition();
     if (pos.hasValue()) {
       for (auto& param: host_parameters) {
         switch (param.parameter_type) {
           case (HostParameterType::Bpm): {
             set_optional_csound_param(csound, param.id, pos->getBpm());
             break;
           }

           case (HostParameterType::TimeInSamples): {
             set_optional_csound_param(csound, param.id, pos->getTimeInSamples());
             break;
           }

           case (HostParameterType::TimeInSeconds): {
             set_optional_csound_param(csound, param.id, pos->getTimeInSeconds());
             break;
           }

           case (HostParameterType::TimeSigNumerator): {
              auto opt_signature = pos->getTimeSignature();
              if (opt_signature.hasValue()) {
                csound->SetControlChannel(param.id.c_str(), static_cast<double>(opt_signature->numerator));
              }
              break;
           }

           case (HostParameterType::TimeSigDenominator): {
              auto opt_signature = pos->getTimeSignature();
              if (opt_signature.hasValue()) {
                csound->SetControlChannel(param.id.c_str(), static_cast<double>(opt_signature->denominator));
              }
              break;
           }

           case (HostParameterType::IsPlaying): {
             csound->SetControlChannel(param.id.c_str(), pos->getIsPlaying() ? 1.0 : 0.0);
             break;
           }

           case (HostParameterType::IsRecording): {
             csound->SetControlChannel(param.id.c_str(), pos->getIsRecording() ? 1.0 : 0.0);
             break;
           }

           case (HostParameterType::IsLooping): {
             csound->SetControlChannel(param.id.c_str(), pos->getIsLooping() ? 1.0 : 0.0);
             break;
           }

           case (HostParameterType::QuarterNotesPosition): {
             set_optional_csound_param(csound, param.id, pos->getPpqPosition());
             break;
           }

           case (HostParameterType::QuarterNotesPositionOfLastBarStart): {
             set_optional_csound_param(csound, param.id, pos->getPpqPositionOfLastBarStart());
             break;
           }

           case (HostParameterType::BarCount): {
             set_optional_csound_param(csound, param.id, pos->getBarCount());
             break;
           }

           case (HostParameterType::FrameRate): {
             auto rate = pos->getFrameRate();
             if (rate.hasValue()) {
               csound->SetControlChannel(param.id.c_str(), rate->getBaseRate());
             }
             break;
           }
         }
       }
     }

  }
}

void Parameters::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream os{destData, true};
    JsonSerializer::serialize(float_audio_parameters, bool_audio_parameters, ui_parameters, os);
}

void Parameters::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream is{data, static_cast<size_t>(sizeInBytes), false};
    const juce::Result result = JsonSerializer::deserialize(is, float_audio_parameters, bool_audio_parameters, ui_parameters);
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
