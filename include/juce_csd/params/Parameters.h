#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <csound/csound.hpp>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <optional>
#include <sys/types.h>
#include <vector>
#include <map>
#include <atomic>
#include <variant>

namespace juce_csd {

/**
 * @brief Defines weather we should smooth parameter changes.
 */
enum class ParameterType {
    Continuous,  // Smoothing is applied to eliminate zipper noize on parameter change
    Discrete,    // No parameter smoothing is applied
};

/**
 * @brief Parameter types which can be read from the host application
 */
enum class HostParameterType {
    Bpm,  //< Reads BPM (beats per minute)
    TimeSigNumerator, //< reads numerator of the time signature
    TimeSigDenominator, //< reads denominator of the time signature
    BarCount, //< current bar count
    QuarterNotesPosition, //< double that denotes play-head position in quarters
    QuarterNotesPositionOfLastBarStart,
    TimeInSamples, //< time in samples
    TimeInSeconds, //< time in seconds
    IsPlaying, //< is host playing
    IsRecording, //< is host recording
    IsLooping, //< is host looping
    FrameRate, //< frame rate for a video
};

namespace {
bool float_equals(float a, float b, float epsilon = 1e-4f) {
    return std::abs(a - b) <= epsilon;
}
}

/*
 * @class SmoothedParam
 * @brief Smoothing of the parameter changes to eliminate zipper noize
 */
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

/// Float parameters for audio control channels that are controlled by UI and host.
// The audio parameters are updated and set to Csound once per processBlock call.
struct AudioParameterFloatSpec {
  std::string id; ///< parameter id (Csound control channel should have the same name)
  std::string name; ///< parameter name as it is displayed in the host
  float min; ///< minimum value
  float max; ///< maximum value
  float step; ///< step of the change
  float default_value; ///< default value
  ParameterType type{ParameterType::Continuous}; ///< should wee apply smoothing
  float smoothing_time_ms{10.0f}; ///< Time for smoothing ramp. Only used if type == Continuous
};

/// Boolean parameters for audio control channels that are controlled by UI and host.
// The audio parameters are updated and set to Csound once per processBlock call.
struct AudioParameterBoolSpec {
  std::string id; ///< parameter id (Csound control channel should have the same name)
  std::string name; ///< parameter name as it is displayed in the host
  bool default_value; ///< default parameter value
};

/// Choice parameters for audio control channels that are controlled by UI and host.
// The audio parameters are updated and set to Csound once per processBlock call.
//
// Choice starts from 1, zero value means nothing is selected. Integer values of 1-based indexes
// are written into Csound control channel.
struct AudioParameterChoiceSpec {
  std::string id; ///< parameter id (Csound control channel should have the same name)
  std::string name; ///< parameter name as it is displayed in the host
  juce::StringArray choices; ///< names for the choices
  int default_value; ///< defaul choice (index is 1-based, 0 means nothing is selected)
};

/// Integer parameters for audio control channels that are controlled by UI and host.
// The audio parameters are updated and set to Csound once per processBlock call.
struct AudioParameterIntSpec {
  std::string id; ///< parameter id (Csound control channel should have the same name)
  std::string name; ///< parameter name as it is displayed in the host
  int min; ///< minimum value
  int max; ///< maximum value
  int step; ///< step of value change
  int default_value; ///< default value
};

/// UI parameters that needs to be persisted
struct UiParameterSpec {
  std::string id; ///< parameter id
  float default_value; ///< default value
};

/// Sensor parameters are values which are read from Csound by UI.
// For example it can be a value for a volume meter.
//
// The sensor parameters are updated and read from Csound once per processBlock call.
// The values of sensor parameters are not persisted
struct SensorParameterSpec {
  std::string id; ///< parameter id (Csound control channel should have the same name)
  float default_value; ///< default value
};

/// Host parameters read values from Host (BPM, play-head position etc.)
struct HostParameterSpec {
  std::string id; ///< parameter id (Csound control channel should have the same name)
  HostParameterType parameter_type; ///< type of the parameter to read from the host
};

/// List of float audio parameters
using AudioParameterFloatList = std::map<std::string, SmoothedParam>;


/// List of boolean audio parameters
using AudioParameterBoolList = std::map<std::string, juce::AudioParameterBool*>;

/// List of choice audio parameters
using AudioParameterChoiceList = std::map<std::string, juce::AudioParameterChoice*>;

/// List of integer audio parameters
using AudioParameterIntList = std::map<std::string, juce::AudioParameterInt*>;

/// Structure of all audio parameters of the plugin
struct AudioParameterList {
    AudioParameterFloatList floats{};
    AudioParameterBoolList bools{};
    AudioParameterChoiceList choices{};
    AudioParameterIntList ints{};
};

/// UI parameter list
using UiParameterList = std::map<std::string, std::atomic<float>>;

/// Sensor parameter list
using SensorParameterList = std::map<std::string, std::atomic<float>>;

/// Host parameter list
using HostParameterList = std::vector<HostParameterSpec>;

/// Parameter specification. It lists all parameters for the application.
struct ParameterSpec {
  std::vector<AudioParameterFloatSpec> audio_floats{}; ///< float audio parameters
  std::vector<AudioParameterBoolSpec> audio_bools{}; ///< boolean audio parameters
  std::vector<AudioParameterChoiceSpec> audio_choices{}; ///< choice audio parameters
  std::vector<AudioParameterIntSpec> audio_ints{}; ///< integer audio parameters
  std::vector<UiParameterSpec> ui{}; ///< UI parameters
  std::vector<SensorParameterSpec> sensor{}; ///< sensor parameters
  std::vector<HostParameterSpec> host{}; ///< parameters which are read from the host
};

/// Parameter pointer
using ParameterPtr = std::variant<
    SmoothedParam*,
    juce::AudioParameterBool*,
    juce::AudioParameterChoice*,
    juce::AudioParameterInt*>;


/// Cached parameter. structure for efficient update of the Csound parameters
// It ensures that parameter update is not triggered over Csound API if values
// has not changed
struct CachedParam {
    std::string id;
    ParameterPtr param_ptr;

    double previous_value{0.0};
    bool has_been_initialized{false};

    bool has_changed(double new_value, double epsilon = 1e-6) const {
        if (!has_been_initialized) return true;
        return std::abs(new_value - previous_value) > epsilon;
    }

    void update_value(double new_value) {
        previous_value = new_value;
        has_been_initialized = true;
    }
};

/// Plugin parameters
class Parameters {
  public:
    Parameters(juce:: AudioProcessor&, const ParameterSpec&);

    /// Method is called on prepareToPlay phase of the plugin
    void prepare(double sample_rate, int max_block_size);

    /// Set audio parameters to Csound and read sensor parameters from Csound
    void update_on_process(Csound* csound, int block_size, juce::AudioPlayHead* play_head);

    /// Serializes the parameters to JSON. Only audio and UI parameters are persisted
    void getStateInformation (juce::MemoryBlock& destData);

    /// Deserializes the parameters from JSON. Only audio and UI parameters are persisted
    void setStateInformation (const void* data, int sizeInBytes);

    /// Gets reference to the float auio parameter by ID
    juce::AudioParameterFloat& get_float_audio_parameter_ref(const std::string&);

    /// Gets reference to the boolean auio parameter by ID
    juce::AudioParameterBool& get_bool_audio_parameter_ref(const std::string&);

    /// Gets reference to the choice auio parameter by ID
    juce::AudioParameterChoice& get_choice_audio_parameter_ref(const std::string&);

    /// Gets reference to the integer auio parameter by ID
    juce::AudioParameterInt& get_int_audio_parameter_ref(const std::string&);

    /// Reads current value of the UI parameter by ID
    std::optional<float> get_ui_parameter(const std::string& id);

    /// Sets current value of the UI parameter by ID
    void set_ui_parameter(const std::string& id, float value);

    /// Reads current value of the sensor. Sensors are updated (read from Csound) once per block
    // processing
    std::optional<float> get_sensor_parameter(const std::string& id);

  private:
    void init_float_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterFloatSpec>&);
    void init_bool_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterBoolSpec>&);
    void init_choice_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterChoiceSpec>&);
    void init_int_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterIntSpec>&);
    void init_ui_parameters(const std::vector<UiParameterSpec>&);
    void init_sensor_parameters(const std::vector<SensorParameterSpec>&);
    void init_host_parameters(const std::vector<HostParameterSpec>&);

    void update_cached_audio_params(Csound* csound, int block_size);
    void update_float_audio_params(Csound* csound, int block_size);
    void update_bool_audio_params(Csound* csound);
    void update_choice_audio_params(Csound* csound);
    void update_int_audio_params(Csound* csound);
    void update_sensor_params(Csound* csound);
    void update_host_params(Csound* csound, juce::AudioPlayHead* play_head);

    AudioParameterList audio_parameters;
    UiParameterList ui_parameters;
    SensorParameterList sensor_parameters;
    HostParameterList host_parameters;
    std::vector<CachedParam> cached_parameters;

    JUCE_DECLARE_NON_COPYABLE(Parameters)
    JUCE_DECLARE_NON_MOVEABLE(Parameters)

};

/// Attachments connect audio parameters with UI elements that control them.
// It allows for bi-directional link between updates of parameters between UI and Csound audio engine.
class ParameterAttachments {
  public:
      ParameterAttachments(Parameters& parameters_): parameters(parameters_) {}

      /// Adds parameter attachment updated with slider (or knob)
      void add_slider(const std::string& name,  juce::Slider& slider) {
          auto attachment = std::make_unique<juce::SliderParameterAttachment>(parameters.get_float_audio_parameter_ref(name), slider);
          slider_attachments.push_back(std::move(attachment));
      }

      /// Adds parameter attachment updated with combo box (drop-down list)
      void add_combo_box(const std::string& name,  juce::ComboBox& combo_box) {
          auto attachment = std::make_unique<juce::ComboBoxParameterAttachment>(parameters.get_choice_audio_parameter_ref(name), combo_box);
          combo_box_attachments.push_back(std::move(attachment));
      }

      /// Adds parameter attachment updated with buttons (toggle)
      void add_button(const std::string& name,  juce::Button& button) {
          auto attachment = std::make_unique<juce::ButtonParameterAttachment>(parameters.get_bool_audio_parameter_ref(name), button);
          button_attachments.push_back(std::move(attachment));
      }

  private:
      Parameters& parameters;
      std::vector<std::unique_ptr<juce::SliderParameterAttachment>> slider_attachments;
      std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> combo_box_attachments;
      std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> button_attachments;
};

}
