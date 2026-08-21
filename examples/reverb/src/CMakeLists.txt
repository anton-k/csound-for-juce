cmake_minimum_required(VERSION 3.24)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(ReverbCsdExample VERSION 0.0.1)

include(../../../cmake/CPM.cmake)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
CPMAddPackage("gh:juce-framework/JUCE#3af3ce009f6a02f6fa651008fffb5b41743a9fab")
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
    # COPY_PLUGIN_AFTER_BUILD TRUE        # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE CsdA               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Rev0                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone CLAP                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME ${PROJECT_NAME})        # The name of the final executable, which can differ from the target name

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
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
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Read the file content
file(READ "csd/reverb.csd" EMBEDDED_CONTENT)

# Configure the header file (replace @EMBEDDED_CONTENT@)
configure_file(
    "embedded_data.h.in"
    "embedded_data.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/csd/reverb.csd"
    "${CMAKE_CURRENT_BINARY_DIR}/csd/reverb.csd"
    COPYONLY
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/csd
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce_csd

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
