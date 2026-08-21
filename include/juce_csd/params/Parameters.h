#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <csound/csound.hpp>
#include <optional>
#include <sys/types.h>
#include <vector>

namespace juce_csd {


using AudioParameterList = std::map<std::string, juce::AudioParameterFloat*>;
using UiParameterList = std::map<std::string, std::atomic<float>>;
using SensorParameterList = std::map<std::string, std::atomic<float>>;

/// Parameters for audio control channels that are controlled by UI and host.
// The audio parameters are updated and set to Csound once per processBlock call.
struct AudioParameterSpec {
  std::string id;
  std::string name;
  float min, max, step, default_value;
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

/// Parameter specification. It lists all parameters for the application.
struct ParameterSpec {
  std::vector<AudioParameterSpec> audio;
  std::vector<UiParameterSpec> ui;
  std::vector<SensorParameterSpec> sensor;
};

class Parameters {
  public:
    Parameters(juce:: AudioProcessor&, const ParameterSpec&);

    /// Set audio parameters to Csound and read sensor parameters from Csound
    void update_on_process(Csound* csound);
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

    AudioParameterList audio_parameters;
    UiParameterList ui_parameters;
    SensorParameterList sensor_parameters;

    JUCE_DECLARE_NON_COPYABLE(Parameters)
    JUCE_DECLARE_NON_MOVEABLE(Parameters)
};

}

