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

ParameterSpecMap::ParameterSpecMap(const ParameterSpec& spec): version(spec.version) {
  for (auto& param: spec.audio_floats) {
    audio_floats.emplace(param.id, param);
  }

  for (auto& param: spec.audio_bools) {
    audio_bools.emplace(param.id, param);
  }

  for (auto& param: spec.audio_ints) {
    audio_ints.emplace(param.id, param);
  }

  for (auto& param: spec.audio_choices) {
    audio_choices.emplace(param.id, param);
  }

  for (auto& param: spec.ui) {
    ui.emplace(param.id, param);
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
    audio_parameters(), ui_parameters(), sensor_parameters(), parameter_spec_map(spec) {
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
  prepare_cached_audio_parameters(csound, sample_rate, max_block_size);
  prepare_cached_host_parameters(csound);
  prepare_sensor_parameters(csound);
  prepare_krate_counter(csound, sample_rate);
}

void Parameters::prepare_krate_counter(Csound* csound, double sample_rate) {
  int ksmps = csound->GetKsmps();
  int current_krate = static_cast<int>(sample_rate) / std::max(1, ksmps);
  krate_divider = std::max(1, current_krate / PARAMETER_SMOOTH_RATE);
  krate_counter = 0;
}

// TODO: do not put in cache paraeters with nullptr as prameter.ptr
void Parameters::prepare_cached_audio_parameters(Csound* csound, double sample_rate, int max_block_size) {
    cached_smoothed_audio_parameters.clear();
    cached_discrete_audio_parameters.clear();

    // Cache float parameters
    for (auto& [id, param] : audio_parameters.floats) {
        param.set_sample_rate(static_cast<float>(sample_rate));
        if (param.type == ParameterType::Continuous) {
          cached_smoothed_audio_parameters.push_back(SmoothedAudioParam(csound, id, &param));
        } else {
          cached_discrete_audio_parameters.push_back(AudioParam{csound, id, ParameterPtr(&param)});
        }
    }

    // Cache bool parameters
    for (auto& [id, param] : audio_parameters.bools) {
        cached_discrete_audio_parameters.push_back(AudioParam{csound, id, ParameterPtr(param)});
    }

    // Cache int parameters
    for (auto& [id, param] : audio_parameters.ints) {
        cached_discrete_audio_parameters.push_back(AudioParam{csound, id, ParameterPtr(param)});
    }

    // Cache choice parameters
    for (auto& [id, param] : audio_parameters.choices) {
        cached_discrete_audio_parameters.push_back(AudioParam{csound, id, ParameterPtr(param)});
    }
}

void Parameters::prepare_cached_host_parameters(Csound* csound) {
  cached_host_parameters.reserve(host_parameters.size());
  cached_host_parameters.clear();
  for (auto& param: host_parameters) {
    cached_host_parameters.push_back(HostParam(csound, param));
  }
}

void Parameters::prepare_sensor_parameters(Csound* csound) {
  sensor_parameter_ptrs.reserve(sensor_parameters.size());
  sensor_parameter_ptrs.clear();
  for (auto& [id, param]: sensor_parameters) {
    sensor_parameter_ptrs.push_back(SensorParam(csound, id, param));
  }
}

void Parameters::update_on_process(juce::AudioPlayHead* play_head) {
  update_audio_params();
  update_sensor_params();
  update_host_params(play_head);
}

void Parameters::update_audio_params() {
  update_smoothed_audio_params();
  update_discrete_audio_params();
}

void Parameters::update_smoothed_audio_params() {
    // 1. Fetch new targets for smoothed parameters (Block-Rate)
    for (auto& smooth_param : cached_smoothed_audio_parameters) {
        if (smooth_param.param && smooth_param.param->param) {
            float new_target = smooth_param.param->param->get();
            smooth_param.param->set_target(new_target);
        }
    }
}

void Parameters::update_discrete_audio_params() {
  for (auto& audio_parameter : cached_discrete_audio_parameters) {
    std::visit([&](auto* param) {
      using ParamType = std::remove_pointer_t<decltype(param)>;

      if (param != nullptr) {
        if constexpr (std::is_same_v<ParamType, SmoothedParam>) {
          // Discrete float parameter (No smoothing)
          if (param->param != nullptr) {
              float new_value = param->param->get();
              audio_parameter.cached.set_value(static_cast<double>(new_value));
          }
        }
        else if constexpr (std::is_same_v<ParamType, juce::AudioParameterBool>) {
            // Boolean parameter
            audio_parameter.cached.set_value(param->get() ? 1.0 : 0.0);
        }
        else if constexpr (std::is_same_v<ParamType, juce::AudioParameterChoice>) {
            // Choice parameter
            audio_parameter.cached.set_value(static_cast<double>(param->getIndex()));
        }
        else if constexpr (std::is_same_v<ParamType, juce::AudioParameterInt>) {
            // Integer parameter
            audio_parameter.cached.set_value(static_cast<double>(param->get()));
        }
      }
    }, audio_parameter.ptr);
  }
}


void CachedInputParam::set_value(double value_to_send) {
    if (has_changed(value_to_send)) {
      if (channel_ptr != nullptr) {
        *static_cast<MYFLT*>(channel_ptr) = static_cast<MYFLT>(value_to_send);
      }
      update_value(value_to_send);
    }
}

double OutputParam::get_value() {
      if (channel_ptr != nullptr) {
        return *static_cast<MYFLT*>(channel_ptr);
      } else {
        return default_value;
      }
}

void Parameters::update_sensor_params() {
  for (auto& param: sensor_parameter_ptrs) {
    param.update();
  }
}

namespace {
  void set_optional_csound_param(CachedInputParam& param, const juce::Optional<double>& value) {
     if (value.hasValue()) {
       param.set_value(*value);
     }
  }
}

void Parameters::update_host_params(juce::AudioPlayHead* play_head) {
  if (host_parameters.empty() || play_head == nullptr) return;

  auto pos_info = play_head->getPosition();
  if (!pos_info.hasValue()) return;
  auto pos = pos_info;

   for (auto& param: cached_host_parameters) {
     switch (param.parameter_type) {
       case (HostParameterType::Bpm): {
         set_optional_csound_param(param.cached, pos->getBpm());
         break;
       }

       case (HostParameterType::TimeInSamples): {
         set_optional_csound_param(param.cached, pos->getTimeInSamples());
         break;
       }

       case (HostParameterType::TimeInSeconds): {
         set_optional_csound_param(param.cached, pos->getTimeInSeconds());
         break;
       }

       case (HostParameterType::TimeSigNumerator): {
          auto opt_signature = pos->getTimeSignature();
          if (opt_signature.hasValue()) {
            param.cached.set_value(static_cast<double>(opt_signature->numerator));
          }
          break;
       }

       case (HostParameterType::TimeSigDenominator): {
          auto opt_signature = pos->getTimeSignature();
          if (opt_signature.hasValue()) {
            param.cached.set_value(static_cast<double>(opt_signature->denominator));
          }
          break;
       }

       case (HostParameterType::IsPlaying): {
         param.cached.set_value(pos->getIsPlaying() ? 1.0 : 0.0);
         break;
       }

       case (HostParameterType::IsRecording): {
         param.cached.set_value(pos->getIsRecording() ? 1.0 : 0.0);
         break;
       }

       case (HostParameterType::IsLooping): {
         param.cached.set_value(pos->getIsLooping() ? 1.0 : 0.0);
         break;
       }

       case (HostParameterType::QuarterNotesPosition): {
         set_optional_csound_param(param.cached, pos->getPpqPosition());
         break;
       }

       case (HostParameterType::QuarterNotesPositionOfLastBarStart): {
         set_optional_csound_param(param.cached, pos->getPpqPositionOfLastBarStart());
         break;
       }

       case (HostParameterType::BarCount): {
         set_optional_csound_param(param.cached, pos->getBarCount());
         break;
       }

       case (HostParameterType::FrameRate): {
         auto rate = pos->getFrameRate();
         if (rate.hasValue()) {
           param.cached.set_value(rate->getBaseRate());
         }
         break;
       }
     }
   }
}

void Parameters::update_krate_params(int ksmps) {
    // 1. Cycle Skipping Logic
    krate_counter++;
    if (krate_counter < krate_divider) {
        return; // Skip this Csound cycle
    }
    krate_counter = 0;
    int step_size = ksmps * krate_divider;

    for (auto& smooth_param : cached_smoothed_audio_parameters) {
        // Only do the math if the parameter is actively ramping
        if (smooth_param.param->samples_remaining > 0) {
            float value_to_send = smooth_param.param->process(step_size);
            smooth_param.cached.set_value(static_cast<double>(value_to_send));
        }
    }
}

void Parameters::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream ooutput_stream{destData, true};
    JsonSerializer::serialize(parameter_spec_map, audio_parameters, ui_parameters, ooutput_stream);
}

void Parameters::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream input_stream{data, static_cast<size_t>(sizeInBytes), false};
    const juce::Result result = JsonSerializer::deserialize(parameter_spec_map, input_stream, audio_parameters, ui_parameters);
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
