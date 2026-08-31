# Project Context
- **Stack:** C++, JUCE, Csound, Cmake.
- **Goal:** Build a library based on JUCE to create plugins which use Csound code over API for audio processing.
         The audio processing and parameter management of the plugin is performed by the library code, the user of the library writes only UI code to define a plugin.
        User specifies the audio processing with Csound file.

# Core Rules (Strictly Enforced)
1. Real-time code should have no memory allocation or perform operations whith unbounded performance time.

# Structure of the library

The library is split on two parts:

- `csd_plugin` namespace defines the core code for audio processing with Csound.
   it should stay indepndent of any JUCE code and defined in terms of Csound API calls.
   Plugin boundaries are kept generic with usage of buffers (real-time fifo queues) for
  audio and midi. Csound processing works with the buffers that should be prefilled
  with data form Host or DAW.

- `juce_csd` namespace defines the code specific to JUCE. It defines
   how to fill Csound audio and midi buffers from/t o DAW and automates
  amangement of the plugin parameters. It defines classes to create plugins based on JUCE
  framework which use Csound for audio and midi processing.

Also examples are defined for the library. The examples show how to build
typical plugins.

# Directory Structure
- `/include`: Include header files for the library
    - `include/csd_plugin` - header files for generic Csound code (audio and midi processing with Csound API)
    - `include/juce_csd` - glue code between generic Csound API code and JUCE.
    - `include/juce_csd/params` - management of plugin parameters
    - `include/juce_csd/audio` - adapts Csound core to JUCE processing of the audio and midi
    - `include/juce_csd/plugin` - class to define the final plugin based on Csound and JUCE

- `/src`:  Implementation of the library. It follows the same directory structure as `include`
   and provides implementation for all ewntities in the `include` directory.

- `/examples`: Examples of the usage of the library.
    - `examples/reverb` - an example of stereo effect plugin (process input stereo signal and produce output stereo-signal, does not process midi)
    - `examples/sine-synth` - an example of a simple pure sine synthesizer with ADSR envelope (reads midi input and produces stereo output)
