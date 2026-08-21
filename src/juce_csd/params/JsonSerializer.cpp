
#include <juce_csd/params/JsonSerializer.hpp>
#include <juce_csd/params/Parameters.h>
#include "juce_core/juce_core.h"
#include <format>
#include <nlohmann/json.hpp>
#include <string>
using json = nlohmann::json;

namespace juce_csd {

// TODO
void JsonSerializer::serialize(const AudioParameterList &audio_parameters, const UiParameterList& ui_parameters, juce::OutputStream &output) {
  json json_data;
  json_data["audio"] = {};
  for (const auto& param: audio_parameters) {
    json_data["audio"][param.first] = param.second->get();
  }

  json_data["ui"] = {};
  for (const auto& param: ui_parameters) {
    json_data["ui"][param.first] = param.second.load();
  }

  std::string json_string = json_data.dump(4);
  output.writeString(json_string);
}

juce::Result JsonSerializer::deserialize(juce::InputStream &input, AudioParameterList &audio_parameters, UiParameterList& ui_parameters) {
  std::string raw_json = input.readEntireStreamAsString().toStdString();
  try {
    json parsed_json = json::parse(raw_json);
    for (auto& audio_param: audio_parameters) {
      *audio_param.second = parsed_json["audio"][audio_param.first];
    }
    for (auto& ui_param: ui_parameters) {
      ui_param.second.store(parsed_json["ui"][ui_param.first]);
    }

    return juce::Result::ok();
  } catch (const json::parse_error& e) {
    return juce::Result::fail(std::format("JSON parsing failed: {}", e.what()));
  } catch (const json::type_error& e) {
    return juce::Result::fail(std::format("JSON parsing failed: {}", e.what()));
  }
}

}
