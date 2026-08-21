cmake_minimum_required(VERSION 3.24)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(../../../cmake/CPM.cmake)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(../../.. ${CMAKE_BINARY_DIR}/juce_csd_build)

set(JUMAKE_TEMPLATE "AudioPlugin")
juce_add_plugin(${PROJECT_NAME}
    # VERSION ...                               # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    COMPANY_NAME CsdArt                          # Specify the name of the plugin's author
    IS_SYNTH FALSE                       # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE               # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE              # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                 # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE    # Does the editor need keyboard focus?
    COPY_PLUGIN_AFTER_BUILD TRUE        # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE CsdA               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Rev0                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone CLAP                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME ${PROJECT_NAME})        # The name of the final executable, which can differ from the target name

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_sources(${PROJECT_NAME}
    PRIVATE
        const.cpp
        PluginEditor.cpp
        PluginProcessor.cpp
    )

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)


# Read the CSD file content at configure time
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/csd/reverb.csd" CSD_FILE_CONTENT)

# Generate a C++ header with the content as a raw string literal
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/EmbeddedCsd.h"
"#pragma once\n#include <string>\n\nnamespace EmbeddedCsd {\n\ninline const std::string& getReverbCsd() {\n    static const std::string data = R\"csd(\n${CSD_FILE_CONTENT}\n)csd\";\n    return data;\n}\n\n} // namespace EmbeddedCsd\n"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce_csd
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
