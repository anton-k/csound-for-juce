cmake_minimum_required(VERSION 3.22)
project(juce_csd VERSION 1.0.0 LANGUAGES C CXX)

# Add this to your root CMakeLists.txt, before any targets
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# ---------------------------------------------------------
# 1. Dependencies (CPM)
# ---------------------------------------------------------
include(cmake/CPM.cmake)

if(NOT TARGET juce::juce_audio_processors)
    CPMAddPackage("gh:juce-framework/JUCE#3af3ce009f6a02f6fa651008fffb5b41743a9fab")
endif()

if(NOT TARGET readerwriterqueue)
    CPMAddPackage(
        NAME readerwriterqueue
        GITHUB_REPOSITORY cameron314/readerwriterqueue
        GIT_TAG 131af2c479c6ba36142ee9805e9f60fc4adfa22c
    )

    # Fallback if the repo's CMakeLists.txt didn't create the target
    if(NOT TARGET readerwriterqueue)
        message(STATUS "readerwriterqueue target not found, creating it manually")
        add_library(readerwriterqueue INTERFACE) # Fixed: removed 'hft_' prefix
        target_include_directories(readerwriterqueue INTERFACE
            ${readerwriterqueue_SOURCE_DIR}
        )
    endif()
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    CPMAddPackage("gh:nlohmann/json#cdf52ae9bef77a0844e02e42df6d2df83a55c4b9")
endif()

# Find Csound
find_library(CSOUND_LIB csound64 REQUIRED)

# ---------------------------------------------------------
# 2. Define Library Target
# ---------------------------------------------------------
add_library(juce_csd STATIC)

target_sources(juce_csd PRIVATE
    src/juce_csd/params/JsonSerializer.cpp
    src/juce_csd/params/Parameters.cpp
    src/juce_csd/audio/FxProcessor.cpp
    src/juce_csd/audio/Buffer.cpp
    src/juce_csd/plugin/FxPluginProcessor.cpp
)

# ---------------------------------------------------------
# 3. Include Directories
# ---------------------------------------------------------
target_include_directories(juce_csd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------------------------------------------------------
# 4. Compile Features & Properties
# ---------------------------------------------------------
# Require C++20 and disable compiler-specific extensions
target_compile_features(juce_csd PUBLIC cxx_std_20)
set_target_properties(juce_csd PROPERTIES
    CXX_EXTENSIONS OFF
    # Crucial for Linux: Allows this static lib to be linked into a shared VST3 plugin
    POSITION_INDEPENDENT_CODE ON
)

# ---------------------------------------------------------
# 5. Link Libraries
# ---------------------------------------------------------
target_link_libraries(juce_csd
    PUBLIC
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_audio_basics

        nlohmann_json::nlohmann_json
        readerwriterqueue

        # Kept PUBLIC so the final plugin automatically links Csound
        ${CSOUND_LIB}
)

target_compile_definitions(juce_csd PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
