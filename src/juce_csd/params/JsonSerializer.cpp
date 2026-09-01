#include <juce_csd/params/JsonSerializer.hpp>
#include <juce_csd/params/Parameters.h>
#include <juce_core/juce_core.h>
#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <algorithm>

using json = nlohmann::json;

namespace juce_csd {

void JsonSerializer::serialize(const ParameterSpecMap& spec, const AudioParameterList &audio_parameters, const UiParameterList& ui_parameters, juce::OutputStream &output) {
    json json_data;
    json_data["version"] = spec.version;
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

    // 3. Choices: save choice as int
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

juce::Result JsonSerializer::deserialize(const ParameterSpecMap& spec, juce::InputStream &input,
AudioParameterList &audio_parameters, UiParameterList& ui_parameters) {
    std::string raw_json = input.readEntireStreamAsString().toStdString();
    if (raw_json.empty()) {
        return juce::Result::fail("Empty state data");
    }

    try {
        json parsed_json = json::parse(raw_json);

        // 1. Read Version & Apply Migrations (if you implement them later)
        int version = 0;
        if (parsed_json.contains("version") && parsed_json["version"].is_number_integer()) {
            version = parsed_json["version"].get<int>();
        }

        // if (version < CURRENT_STATE_VERSION) { migrate_state(parsed_json, version); }

        // 2. Resilient Audio Parameter Restoration
        if (parsed_json.contains("audio") && parsed_json["audio"].is_object()) {
            auto& audio_json = parsed_json["audio"];

            // --- FLOATS ---
            for (auto& [id, param] : audio_parameters.floats) {
                if (param.param == nullptr) continue;

                // Safely get default from spec map (no exceptions)
                float normalized_value = 0.0f;
                auto spec_it = spec.audio_floats.find(id);
                if (spec_it != spec.audio_floats.end()) {
                    normalized_value = param.param->convertTo0to1(spec_it->second.default_value);
                }

                // Use is_number() to accept both 1.0 and 1
                if (audio_json.contains(id) && audio_json[id].is_number()) {
                    normalized_value = std::clamp(audio_json[id].get<float>(), 0.0f, 1.0f);
                }

                param.param->setValueNotifyingHost(normalized_value);
                param.set_target(param.param->convertFrom0to1(normalized_value), true); // force instant
            }

            // --- BOOLS ---
            for (auto& [id, param_ptr] : audio_parameters.bools) {
                if (param_ptr == nullptr) continue;

                float normalized_value = 0.0f;
                auto spec_it = spec.audio_bools.find(id);
                if (spec_it != spec.audio_bools.end()) {
                    normalized_value = spec_it->second.default_value ? 1.0f : 0.0f;
                }

                if (audio_json.contains(id) && audio_json[id].is_number()) {
                    normalized_value = std::clamp(audio_json[id].get<float>(), 0.0f, 1.0f);
                }

                param_ptr->setValueNotifyingHost(normalized_value);
            }

            // --- INTS ---
            for (auto& [id, param_ptr] : audio_parameters.ints) {
                if (param_ptr == nullptr) continue;

                float normalized_value = 0.0f;
                auto spec_it = spec.audio_ints.find(id);
                if (spec_it != spec.audio_ints.end()) {
                    normalized_value =
param_ptr->convertTo0to1(static_cast<float>(spec_it->second.default_value));
                }

                if (audio_json.contains(id) && audio_json[id].is_number()) {
                    normalized_value = std::clamp(audio_json[id].get<float>(), 0.0f, 1.0f);
                }

                param_ptr->setValueNotifyingHost(normalized_value);
            }

            // --- CHOICES ---
            for (auto& [id, param_ptr] : audio_parameters.choices) {
                if (param_ptr == nullptr) continue;

                float normalized_value = 0.0f;
                auto spec_it = spec.audio_choices.find(id);
                if (spec_it != spec.audio_choices.end()) {
                    normalized_value =
param_ptr->getNormalisableRange().convertTo0to1(static_cast<float>(spec_it->second.default_value));
                }

                if (audio_json.contains(id) && audio_json[id].is_number_integer()) {
                    int index = audio_json[id].get<int>();
                    float raw_norm = param_ptr->getNormalisableRange().convertTo0to1(static_cast<float>(index));
                    normalized_value = std::clamp(raw_norm, 0.0f, 1.0f);
                }

                param_ptr->setValueNotifyingHost(normalized_value);
            }
        }

        // 3. Resilient UI Parameter Restoration
        if (parsed_json.contains("ui") && parsed_json["ui"].is_object()) {
            auto& ui_json = parsed_json["ui"];
            for (auto& [id, atomic_val] : ui_parameters) {

                // Safely get default from spec map
                float default_val = 0.0f;
                auto spec_it = spec.ui.find(id);
                if (spec_it != spec.ui.end()) {
                    default_val = spec_it->second.default_value;
                }

                if (ui_json.contains(id) && ui_json[id].is_number()) {
                    atomic_val.store(ui_json[id].get<float>());
                } else {
                    atomic_val.store(default_val);
                }
            }
        }

        return juce::Result::ok();

    } catch (const json::parse_error& e) {
        return juce::Result::fail(std::format("JSON parsing failed: {}", e.what()));
    } catch (const json::type_error& e) {
        return juce::Result::fail(std::format("JSON type error: {}", e.what()));
    } catch (const std::exception& e) {
        return juce::Result::fail(std::format("State deserialization failed: {}", e.what()));
    }
}

} // namespace juce_csd
