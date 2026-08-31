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

namespace {
bool float_equals(float a, float b, float epsilon = 1e-4f) {
    return std::abs(a - b) <= epsilon;
}
}

SmoothedParam::SmoothedParam(juce::AudioParameterFloat* p, ParameterType t, float default_val, float smooth_ms)
        : param(p), type(t), smoothing_time_ms(smooth_ms), current_value(default_val), target_value(default_val)
{}

void SmoothedParam::set_sample_rate(float sample_rate) {
    if (type == ParameterType::Continuous && smoothing_time_ms > 0.0f) {
        smoothing_samples = static_cast<int>(sample_rate * (smoothing_time_ms / 1000.0f));
        if (smoothing_samples < 1) smoothing_samples = 1;
    } else {
        smoothing_samples = 0;
    }
}

void SmoothedParam::set_target(float new_target, bool force_instant) {
    if (!float_equals(new_target, target_value) || force_instant) {
        target_value = new_target;

        if (force_instant || type != ParameterType::Continuous || smoothing_samples == 0) {
            current_value = target_value;
            samples_remaining = 0;
        } else {
            samples_remaining = smoothing_samples;
            increment = (target_value - current_value) / static_cast<float>(smoothing_samples);
        }
    }
}

float SmoothedParam::process(int block_size) {
    if (samples_remaining > 0) {
        // Advance by the number of samples in this block (or whatever is left)
        int steps = std::min(samples_remaining, block_size);

        current_value += increment * static_cast<float>(steps);
        samples_remaining -= steps;

        if (samples_remaining <= 0) {
            current_value = target_value; // Snap to exact target at the end
            samples_remaining = 0;
        }
    }
    return current_value;
}

Parameters::Parameters(juce::AudioProcessor& processor, const ParameterSpec& spec):
    audio_parameters(), ui_parameters(), sensor_parameters() {
    init_float_audio_parameters(processor, spec.audio_floats);
    init_bool_audio_parameters(processor, spec.audio_bools);
    init_choice_audio_parameters(processor, spec.audio_choices);
    init_int_audio_parameters(processor, spec.audio_ints);
    init_ui_parameters(spec.ui);
    init_sensor_parameters(spec.sensor);
}

juce::AudioParameterFloat& Parameters::get_float_audio_parameter_ref(const std::string& id) {
    auto it = audio_parameters.floats.find(id);
    if (it != audio_parameters.floats.end() && it->second.param != nullptr) {
        return *(it->second.param);
    }
    throw std::runtime_error("Float parameter not found: " + id);
}

juce::AudioParameterBool& Parameters::get_bool_audio_parameter_ref(const std::string& id) {
    auto it = audio_parameters.bools.find(id);
    if (it != audio_parameters.bools.end() && it->second != nullptr) {
        return *(it->second);
    }
    throw std::runtime_error("Bool parameter not found: " + id);
}

juce::AudioParameterChoice& Parameters::get_choice_audio_parameter_ref(const std::string& id) {
    auto it = audio_parameters.choices.find(id);
    if (it != audio_parameters.choices.end() && it->second != nullptr) {
        return *(it->second);
    }
    throw std::runtime_error("Choice parameter not found: " + id);
}

juce::AudioParameterInt& Parameters::get_int_audio_parameter_ref(const std::string& id) {
    auto it = audio_parameters.ints.find(id);
    if (it != audio_parameters.ints.end() && it->second != nullptr) {
        return *(it->second);
    }
    throw std::runtime_error("Int parameter not found: " + id);
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
        audio_parameters.floats.emplace(spec.id, SmoothedParam(param, spec.type, spec.default_value, spec.smoothing_time_ms));
    }
}

void Parameters::init_bool_audio_parameters(juce::AudioProcessor& processor, const std::vector<AudioParameterBoolSpec>& param_specs) {
    for (const auto& spec : param_specs) {
        DBG("Creating boolean parameter: " << spec.name);
        auto* param = new juce::AudioParameterBool(spec.id, spec.name, spec.default_value);

        // Add to processor
        processor.addParameter(param);

        // Store pointer
        audio_parameters.bools.emplace(spec.id, param);
    }
}

void Parameters::init_choice_audio_parameters(juce::AudioProcessor& processor, const std::vector<AudioParameterChoiceSpec>& param_specs) {
    for (const auto& spec : param_specs) {
        DBG("Creating choice parameter: " << spec.name);
        auto* param = new juce::AudioParameterChoice(spec.id, spec.name, spec.choices, spec.default_value);

        // Add to processor
        processor.addParameter(param);

        // Store pointer
        audio_parameters.choices.emplace(spec.id, param);
    }
}

void Parameters::init_int_audio_parameters(juce::AudioProcessor& processor, const std::vector<AudioParameterIntSpec>& param_specs) {
    for (const auto& spec : param_specs) {
        DBG("Creating integer parameter: " << spec.name);
        auto* param = new juce::AudioParameterInt(spec.id, spec.name, spec.min, spec.max, spec.default_value);

        // Add to processor
        processor.addParameter(param);

        // Store pointer
        audio_parameters.ints.emplace(spec.id, param);
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

void Parameters::prepare(Csound* csound, double sample_rate, int max_block_size) {
    cached_parameters.reserve(
        audio_parameters.floats.size() +
        audio_parameters.bools.size() +
        audio_parameters.choices.size() +
        audio_parameters.ints.size()
    );
    cached_parameters.clear();

    // Cache float parameters
    for (auto& [id, param] : audio_parameters.floats) {
        param.set_sample_rate(static_cast<float>(sample_rate));
        cached_parameters.push_back(CachedParam{csound, id, ParameterPtr(&param)});
    }

    // Cache bool parameters
    for (auto& [id, param] : audio_parameters.bools) {
        cached_parameters.push_back(CachedParam{csound, id, ParameterPtr(param)});
    }

    // Cache int parameters
    for (auto& [id, param] : audio_parameters.ints) {
        cached_parameters.push_back(CachedParam{csound, id, ParameterPtr(param)});
    }

    // Cache choice parameters
    for (auto& [id, param] : audio_parameters.choices) {
        cached_parameters.push_back(CachedParam{csound, id, ParameterPtr(param)});
    }
}

void Parameters::update_on_process(Csound* csound, int block_size, juce::AudioPlayHead* play_head) {
  update_cached_audio_params(csound, block_size);
  update_sensor_params(csound);
  update_host_params(csound, play_head);
}

void Parameters::update_cached_audio_params(Csound* csound, int block_size) {
  for (auto& cached : cached_parameters) {
    std::visit([&](auto* param) {
      using ParamType = std::remove_pointer_t<decltype(param)>;

      if (param != nullptr) {
        if constexpr (std::is_same_v<ParamType, SmoothedParam>) {
          // Float parameter with smoothing
          if (param->param != nullptr) {
              float new_target = param->param->get();
              param->set_target(new_target);
              cached.set_value(csound, param->process(block_size));
          }
        }
        else if constexpr (std::is_same_v<ParamType, juce::AudioParameterBool>) {
            // Boolean parameter
            cached.set_value(csound, param->get() ? 1.0 : 0.0);
        }
        else if constexpr (std::is_same_v<ParamType, juce::AudioParameterChoice>) {
            // Choice parameter
            cached.set_value(csound, static_cast<double>(param->getIndex()));
        }
        else if constexpr (std::is_same_v<ParamType, juce::AudioParameterInt>) {
            // Integer parameter
            cached.set_value(csound, static_cast<double>(param->get()));
        }
      }
    }, cached.param_ptr);
  }
}

void CachedParam::set_value(Csound* csound, double value_to_send) {
    // Only call Csound API if value has changed
    if (has_changed(value_to_send)) {
      if (channel_ptr != nullptr) {
        *static_cast<MYFLT*>(channel_ptr) = static_cast<MYFLT>(value_to_send);
      } else {
        csound->SetControlChannel(id.c_str(), value_to_send);
      }
      update_value(value_to_send);
    }
}

void Parameters::update_float_audio_params(Csound* csound, int block_size) {
  for (auto& param: audio_parameters.floats) {
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
  for (auto& param: audio_parameters.bools) {
    if (param.second != nullptr) {
      csound->SetControlChannel(param.first.c_str(), (param.second->get()? 1.0: 0.0));
    }
  }
}

void Parameters::update_choice_audio_params(Csound* csound) {
  for (auto& param: audio_parameters.choices) {
    if (param.second != nullptr) {
      csound->SetControlChannel(param.first.c_str(), static_cast<double>(param.second->getIndex()));
    }
  }
}

void Parameters::update_int_audio_params(Csound* csound) {
  for (auto& param: audio_parameters.ints) {
    if (param.second != nullptr) {
      csound->SetControlChannel(param.first.c_str(), static_cast<double>(param.second->get()));
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
  if (host_parameters.empty() || play_head == nullptr) return;

  auto pos_info = play_head->getPosition();
  if (!pos_info.hasValue()) return;
  auto pos = pos_info;

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
