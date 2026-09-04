#pragma once

#include <csound/csound.hpp>
#include <string>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_csd/params/Parameters.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <csd_plugin/audio/Processor.h>
#include <csd_plugin/audio/Logger.h>
#include <LockFreeSpscQueue.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace juce_csd {

enum class LogSource { Csound, Custom };

struct LogMessage {
    csd_plugin::LogLevel level;
    LogSource source;
    char text[512];
};

class CsoundLogConsumer;

/// Class for the audio processor which runs the Csound.
// Value of this class can be used to define all methods of the plugin which relies
// on Csound audio processing.
class Processor {
  friend class CsoundLogConsumer;

  public:
    /// The processor is initialized with CSD file content, layout of IO-busses, specification of the parameters and reference to the JUCE audio processor class.
    Processor(const std::string& csd, const csd_plugin::IOLayout&, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor);
    ~Processor() = default;

    /// Called on main thread to prepare plugin for audio processing
    void prepareToPlay(double sampleRate, int maxBlockSize);

    /// Called on audio-thread to process audio in blocks
    void processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>&, juce::MidiBuffer&);

    /// Called when audio processing is over to release resources
    void releaseResources();

    /// Latency of the processor in samples.
    // For plugins which process no audio input the latency is zero, and for
    // plugins which do process inputs the latency equals to ksmps.
    int get_latency_samples();

    /// Serialize the parameters of the plugin to JSON
    void getStateInformation (juce::MemoryBlock& destData);

    /// Deserialize the parameters of the plugin from JSON
    void setStateInformation (const void* data, int sizeInBytes);

    /// Get plugin parameters
    Parameters& get_parameters();

    /// Get IO-layout of the plugin
    const csd_plugin::IOLayout& get_io_layout() const;

    /// RT-safe custom logging for the plugin developer.
    // Must pass a C-string (e.g., string literal or .toRawUTF8()) to avoid allocations.
    void log(csd_plugin::LogLevel level, const char* text);

    std::unique_ptr<CsoundLogConsumer> create_log_consumer();

    bool is_csound_valid() const {
        return csound.is_csound_valid();
    }

    std::string get_last_error() const {
        return csound.get_last_error();
    }

  private:
    void read_midi_from_host(juce::MidiBuffer&, int64_t block_start_global_sample);
    void write_midi_to_host(juce::MidiBuffer&, int64_t block_start_sample, int block_size);
    void read_input_buffer_from_host(juce::AudioBuffer<float>&);
    void write_output_buffer_to_host(juce::AudioBuffer<float>&);
    bool pop_log(LogMessage& msg);

    std::vector<MYFLT> host_input_scratch;
    std::vector<MYFLT> host_output_scratch;

    csd_plugin::Processor csound;
    Parameters parameters;

    // 1. User-owned buffer (allocated once, never resized)
    std::array<LogMessage, 1024> log_buffer;

    // 2. The queue manager (manages indices, prevents false sharing)
    LockFreeSpscQueue<LogMessage> log_queue;

    bool was_bypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

}
