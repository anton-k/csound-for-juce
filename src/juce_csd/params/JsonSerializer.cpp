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

    // 1. Floats: Get actual value, then convert to normalized (0.0 - 1.0)
    for (const auto& pair : audio_parameters.floats) {
        if (pair.second.param != nullptr) {
            float actual_value = pair.second.param->get();
            float normalized_value = pair.second.param->convertTo0to1(actual_value);
            json_data["audio"][pair.first] = normalized_value;
        }
    }

    // 2. Bools: Convert to 0.0f or 1.0f
    for (const auto& pair : audio_parameters.bools) {
        if (pair.second != nullptr) {
            json_data["audio"][pair.first] = pair.second->get() ? 1.0f : 0.0f;
        }
    }

    // 3. Choices: Use the NormalisableRange to convert the 0-based index to 0.0 - 1.0
    for (const auto& pair : audio_parameters.choices) {
        if (pair.second != nullptr) {
            json_data["audio"][pair.first] = pair.second->getIndex();
        }
    }

    // 4. Ints: Get actual value, then convert to normalized (0.0 - 1.0)
    for (const auto& pair : audio_parameters.ints) {
        if (pair.second != nullptr) {
            float actual_value = static_cast<float>(pair.second->get());
            float normalized_value = pair.second->convertTo0to1(actual_value);
            json_data["audio"][pair.first] = normalized_value;
        }
    }

    // 5. UI Parameters
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

        if (parsed_json.contains("audio") && parsed_json["audio"].is_object()) {

            // Floats
            for (auto& pair : audio_parameters.floats) {
                if (pair.second.param == nullptr) continue;
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    float normalized_value = parsed_json["audio"][id].get<float>();

                    // setValueNotifyingHost ALWAYS expects a normalized value (0.0 to 1.0)
                    pair.second.param->setValueNotifyingHost(normalized_value);

                    // Update your custom SmoothedParam with the actual (denormalized) value
                    float actual_value = pair.second.param->convertFrom0to1(normalized_value);
                    pair.second.set_target(actual_value, true); // force instant
                }
            }

            // Bools
            for (auto& pair : audio_parameters.bools) {
                if (pair.second == nullptr) continue;
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    float normalized_value = parsed_json["audio"][id].get<float>();
                    pair.second->setValueNotifyingHost(normalized_value);
                }
            }

            // Ints
            for (auto& pair : audio_parameters.ints) {
                if (pair.second == nullptr) continue;
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    float normalized_value = parsed_json["audio"][id].get<float>();
                    pair.second->setValueNotifyingHost(normalized_value);
                }
            }

            // Choices
            for (auto& pair : audio_parameters.choices) {
                if (pair.second == nullptr) continue;
                const std::string& id = pair.first;
                if (parsed_json["audio"].contains(id)) {
                    int index = parsed_json["audio"][id].get<int>();
                    float normalized_value =
                        pair.second->getNormalisableRange().convertTo0to1(static_cast<float>(index));
                    pair.second->setValueNotifyingHost(normalized_value);
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
