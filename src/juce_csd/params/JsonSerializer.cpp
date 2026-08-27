#include <juce_csd/params/JsonSerializer.hpp>
#include <juce_csd/params/Parameters.h>
#include <juce_core/juce_core.h>
#include <format>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace juce_csd {

void JsonSerializer::serialize(const AudioParameterList &audio_parameters, const UiParameterList& ui_parameters, juce::OutputStream &output) {
    json json_data;

    json_data["audio"] = json::object();
    for (const auto& pair : audio_parameters.floats) {
        if (pair.second.param != nullptr) {
            json_data["audio"][pair.first] = pair.second.param->get();
        }
    }

    for (const auto& pair : audio_parameters.bools) {
        if (pair.second != nullptr) {
            json_data["audio"][pair.first] = pair.second->get();
        }
    }

    for (const auto& pair : audio_parameters.choices) {
        if (pair.second != nullptr) {
            json_data["audio"][pair.first] = pair.second->getIndex();
        }
    }

    for (const auto& pair : audio_parameters.ints) {
        if (pair.second != nullptr) {
            json_data["audio"][pair.first] = pair.second->get();
        }
    }


    json_data["ui"] = json::object();
    for (const auto& pair : ui_parameters) {
        json_data["ui"][pair.first] = pair.second.load();
    }

    std::string json_string = json_data.dump(4);
    output.writeString(juce::String(json_string));
}

juce::Result JsonSerializer::deserialize(juce::InputStream &input, AudioParameterList &audio_parameters, UiParameterList& ui_parameters) {
    std::string raw_json = input.readEntireStreamAsString().toStdString();
    if (raw_json.empty()) {
        return juce::Result::fail("Empty state data");
    }

    try {
        json parsed_json = json::parse(raw_json);

        // Restore Audio Parameters
        if (parsed_json.contains("audio") && parsed_json["audio"].is_object()) {
            for (auto& pair : audio_parameters.floats) {
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    float saved_value = parsed_json["audio"][id].get<float>();

                    // CRITICAL: Force instant update to prevent smoothing ramps when loading presets
                    pair.second.param->setValueNotifyingHost(saved_value);
                    pair.second.set_target(saved_value, true); // true = force instant
                }
            }

            for (auto& pair : audio_parameters.bools) {
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    float saved_value = parsed_json["audio"][id].get<bool>();
                    pair.second->setValueNotifyingHost(saved_value);
                }
            }

            for (auto& pair : audio_parameters.ints) {
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    float saved_value = parsed_json["audio"][id].get<int>();
                    pair.second->setValueNotifyingHost(saved_value);
                }
            }

            for (auto& pair : audio_parameters.choices) {
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    float saved_value = parsed_json["audio"][id].get<int>();
                    pair.second->setValueNotifyingHost(saved_value);
                }
            }
        }

        // Restore UI Parameters
        if (parsed_json.contains("ui") && parsed_json["ui"].is_object()) {
            for (auto& pair : ui_parameters) {
                const std::string& id = pair.first;
                if (parsed_json["ui"].contains(id)) {
                    pair.second.store(parsed_json["ui"][id].get<float>());
                }
            }
        }

        return juce::Result::ok();
    } catch (const json::parse_error& e) {
        return juce::Result::fail(std::format("JSON parsing failed: {}", e.what()));
    } catch (const json::type_error& e) {
        return juce::Result::fail(std::format("JSON type error: {}", e.what()));
    }
}

} // namespace juce_csd
