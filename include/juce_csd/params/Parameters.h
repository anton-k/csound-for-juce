#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <csound/csound.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <optional>
#include <sys/types.h>
#include <vector>
#include <map>
#include <atomic>

namespace juce_csd {

enum class ParameterType {
    Continuous,  // Knobs, sliders - smoothing is beneficial
    Discrete,    // Toggles, switches, step selectors - smoothing breaks `changed` opcode
    Boolean      // Special case of discrete (0 or 1)
};

enum class HostParameterType {
    Bpm,
    TimeSigNumerator,
    TimeSigDenominator,
    BarCount,
    QuarterNotesPosition,
    QuarterNotesPositionOfLastBarStart,
    TimeInSamples,
    TimeInSeconds,
    IsPlaying,
    IsRecording,
    FrameRate,
    IsLooping,
};

namespace {
bool float_equals(float a, float b, float epsilon = 1e-4f) {
    return std::abs(a - b) <= epsilon;
}
}

struct SmoothedParam {
    juce::AudioParameterFloat* param{nullptr};
    ParameterType type{ParameterType::Continuous};
    float smoothing_time_ms{10.0f};

    float current_value{0.0f};
    float target_value{0.0f};
    float increment{0.0f};
    int samples_remaining{0};
    int smoothing_samples{441};

    SmoothedParam(juce::AudioParameterFloat* p, ParameterType t, float default_val, float smooth_ms)
        : param(p), type(t), smoothing_time_ms(smooth_ms), current_value(default_val), target_value(default_val) {}

    void set_sample_rate(float sample_rate) {
        if (type == ParameterType::Continuous && smoothing_time_ms > 0.0f) {
            smoothing_samples = static_cast<int>(sample_rate * (smoothing_time_ms / 1000.0f));
            if (smoothing_samples < 1) smoothing_samples = 1;
        } else {
            smoothing_samples = 0;
        }
    }

    void set_target(float new_target, bool force_instant = false) {
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

    float process(int block_size) {
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
};

/// Parameters for audio control channels that are controlled by UI and host.
// The audio parameters are updated and set to Csound once per processBlock call.
struct AudioParameterSpec {
  std::string id;
  std::string name;
  float min, max, step, default_value;
  ParameterType type{ParameterType::Continuous};
  float smoothing_time_ms{10.0f}; // Only used if type == Continuous
};

/// UI parameters that needs to be persisted
struct UiParameterSpec {
  std::string id;
  float default_value;
};

/// Sensor parameters are values which are read from Csound by UI.
// For example it can be a value for a volume meter.
//
// The sensor parameters are updated and read from Csound once per processBlock call.
struct SensorParameterSpec {
  std::string id;
  float default_value;
};

struct HostParameterSpec {
  std::string id;
  HostParameterType parameter_type;
};

using AudioParameterList = std::map<std::string, SmoothedParam>;
using UiParameterList = std::map<std::string, std::atomic<float>>;
using SensorParameterList = std::map<std::string, std::atomic<float>>;
using HostParameterList = std::vector<HostParameterSpec>;

/// Parameter specification. It lists all parameters for the application.
struct ParameterSpec {
  std::vector<AudioParameterSpec> audio;
  std::vector<UiParameterSpec> ui{};
  std::vector<SensorParameterSpec> sensor{};
  std::vector<HostParameterSpec> host{};
};

class Parameters {
  public:
    Parameters(juce:: AudioProcessor&, const ParameterSpec&);

    void prepare(double sample_rate, int max_block_size);

    /// Set audio parameters to Csound and read sensor parameters from Csound
    void update_on_process(Csound* csound, int block_size, juce::AudioPlayHead* play_head);
    void getStateInformation (juce::MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);

    juce::AudioParameterFloat& get_audio_parameter_ref(const std::string&);

    std::optional<float> get_ui_parameter(const std::string& id);
    void set_ui_parameter(const std::string& id, float value);
    std::optional<float> get_sensor_parameter(const std::string& id);

  private:
    void init_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterSpec>&);
    void init_ui_parameters(const std::vector<UiParameterSpec>&);
    void init_sensor_parameters(const std::vector<SensorParameterSpec>&);
    void init_host_parameters(const std::vector<HostParameterSpec>&);

    void update_audio_params(Csound* csound, int block_size);
    void update_sensor_params(Csound* csound);
    void update_host_params(Csound* csound, juce::AudioPlayHead* play_head);

    AudioParameterList audio_parameters;
    UiParameterList ui_parameters;
    SensorParameterList sensor_parameters;
    HostParameterList host_parameters;

    JUCE_DECLARE_NON_COPYABLE(Parameters)
    JUCE_DECLARE_NON_MOVEABLE(Parameters)

};

class ParameterAttachments {
  public:
      ParameterAttachments(Parameters& parameters_): parameters(parameters_) {}

      void add_slider(const std::string& name,  juce::Slider& slider) {
          auto attachment = std::make_unique<juce::SliderParameterAttachment>(parameters.get_audio_parameter_ref(name), slider);
          slider_attachments.push_back(std::move(attachment));
      }

      void add_combo_box(const std::string& name,  juce::ComboBox& combo_box) {
          auto attachment = std::make_unique<juce::ComboBoxParameterAttachment>(parameters.get_audio_parameter_ref(name), combo_box);
          combo_box_attachments.push_back(std::move(attachment));
      }

      void add_button(const std::string& name,  juce::Button& button) {
          auto attachment = std::make_unique<juce::ButtonParameterAttachment>(parameters.get_audio_parameter_ref(name), button);
          button_attachments.push_back(std::move(attachment));
      }

  private:
      Parameters& parameters;
      std::vector<std::unique_ptr<juce::SliderParameterAttachment>> slider_attachments;
      std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> combo_box_attachments;
      std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> button_attachments;

};

}
