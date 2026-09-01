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

    SmoothedParam(juce::AudioParameterFloat* p, ParameterType t, float default_val, float smooth_ms);
    void set_sample_rate(float sample_rate);
    void set_target(float new_target, bool force_instant = false);
    float process(int block_size);
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
//
// TODO: consider using: std::map<std::string, std::atomic<float>>;
// see note: [1.0]
using UiParameterList = std::map<std::string, std::atomic<float>>;

/// Sensor parameter list
//
// TODO: consider using: std::map<std::string, std::atomic<float>>;using SensorParameterList = std::map<std::string, std::atomic<float>>;
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


using CsoundChannelPtr = void*;

/// Cached parameter. structure for efficient update of the Csound parameters
// It ensures that parameter update is not triggered over Csound API if values
// has not changed
struct CachedInputParam {
    std::string id;
    CsoundChannelPtr channel_ptr{nullptr};

    CachedInputParam(Csound* csound, const std::string& id_): id(id_) {
      // TODO: for sensor channels we should use CSOUND_OUTPUT_CHANNEL
      // and for host params we should use CSOUND_INPUT_CHANNEL
        int status = csound->GetChannelPtr(channel_ptr, id.c_str(), CSOUND_CONTROL_CHANNEL | CSOUND_INPUT_CHANNEL);
        if (status != 0) {
          channel_ptr = nullptr;
        }
    }

    double previous_value{0.0};
    bool has_been_initialized{false};

    bool has_changed(double new_value, double epsilon = 1e-6) const {
        if (!has_been_initialized) return true;
        return std::abs(new_value - previous_value) > epsilon;
    }

    void set_value(Csound* csound, double new_value);

    void update_value(double new_value) {
        previous_value = new_value;
        has_been_initialized = true;
    }
};

struct AudioParam {
  ParameterPtr ptr;
  CachedInputParam cached;

  AudioParam(Csound* csound, const std::string& id, ParameterPtr param_ptr_):
    ptr(param_ptr_), cached(csound, id) {};
};

struct HostParam {
  HostParameterType parameter_type;
  CachedInputParam cached;

  HostParam(Csound* csound, const HostParameterSpec& spec):
    parameter_type(spec.parameter_type),
    cached(csound, spec.id)
  {}
};

/// Plugin parameters
class Parameters {
  public:
    Parameters(juce:: AudioProcessor&, const ParameterSpec&);

    /// Method is called on prepareToPlay phase of the plugin
    void prepare(Csound* csound, double sample_rate, int max_block_size);

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
    void prepare_cached_audio_parameters(Csound* csound, double sample_rate, int max_block_size);
    void prepare_cached_host_parameters(Csound* csound);

    void init_float_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterFloatSpec>&);
    void init_bool_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterBoolSpec>&);
    void init_choice_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterChoiceSpec>&);
    void init_int_audio_parameters(juce::AudioProcessor&, const std::vector<AudioParameterIntSpec>&);
    void init_ui_parameters(const std::vector<UiParameterSpec>&);
    void init_sensor_parameters(const std::vector<SensorParameterSpec>&);
    void init_host_parameters(const std::vector<HostParameterSpec>&);

    void update_audio_params(Csound* csound, int block_size);
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

    std::vector<AudioParam> cached_audio_parameters{};
    std::vector<HostParam> cached_host_parameters{};

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

/* Notes

 # 1.0. usage of std:map with atomic

 std::atomic is neither copyable nor movable. While your use of emplace() and
std::piecewise_construct in the .cpp file correctly constructs these in-place without
triggering copies, you must be careful never to pass these maps by value or use STL algorithms
that require copying/moving elements.

 • Verdict: It is safe as currently written, but if you ever refactor this, consider using
   std::map<std::string, std::unique_ptr<std::atomic<float>>> or a custom thread-safe wrapper
   to avoid accidental compilation errors later.
*/
