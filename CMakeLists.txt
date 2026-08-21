# In your library's CMakeLists.txt
cmake_minimum_required(VERSION 3.22)
project(juce_csd VERSION 1.0.0 LANGUAGES C CXX)

# Define your library target
add_library(juce_csd)

# Add your source files (if any)
target_sources(juce_csd PRIVATE
  src/juce_csd/params/JsonSerializer.cpp
  src/juce_csd/params/Parameters.cpp
  src/juce_csd/audio/FxProcessor.cpp
  src/juce_csd/audio/Buffer.cpp
  src/juce_csd/plugin/FxPluginProcessor.cpp
)

include(cmake/CPM.cmake)
CPMAddPackage("gh:juce-framework/JUCE#3af3ce009f6a02f6fa651008fffb5b41743a9fab")
CPMAddPackage(
    NAME readerwriterqueue
    GITHUB_REPOSITORY cameron314/readerwriterqueue
    GIT_TAG 131af2c479c6ba36142ee9805e9f60fc4adfa22c
)
# Check if the target exists, if not, create it
if(NOT TARGET readerwriterqueue)
    message(STATUS "readerwriterqueue target not found, creating it manually")
    add_library(hft_readerwriterqueue INTERFACE)
    target_include_directories(readerwriterqueue INTERFACE
        ${readerwriterqueue_SOURCE_DIR}
    )
endif()

get_target_property(INC_DIRS readerwriterqueue INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "readerwriterqueue include dirs: ${INC_DIRS}")

CPMAddPackage("gh:nlohmann/json#cdf52ae9bef77a0844e02e42df6d2df83a55c4b9")

target_include_directories(juce_csd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}>
)

# If you have private includes
target_include_directories(juce_csd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_library(CSOUND_LIB csound64 REQUIRED)

target_link_libraries(juce_csd
  PUBLIC
        juce::juce_audio_basics  # Includes gui_basics, audio_processors, etc.
        nlohmann_json::nlohmann_json
        readerwriterqueue
        ${CSOUND_LIB}
)

# Set C++ standard
target_compile_features(juce_csd PUBLIC cxx_std_20)
