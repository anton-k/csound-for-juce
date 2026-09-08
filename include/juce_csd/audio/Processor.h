#pragma once

#include <LockFreeSpscQueue.h>
#include <array>
#include <atomic>
#include <chrono>
#include <csd_plugin/audio/Logger.h>
#include <csd_plugin/audio/Processor.h>
#include <csound/csound.hpp>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_csd/params/Parameters.h>
#include <memory>
#include <string>
#include <thread>

namespace juce_csd {

enum class LogSource { Csound, Custom };

struct LogMessage {
  csd_plugin::LogLevel level;
  LogSource source;
  char text[512];
};

class CsoundLogConsumer;

enum class ProcessorStage {
  Free,
  PrepareToPlay,
  ProcessBlock,
  ReleaseResources
};

static_assert(std::atomic<ProcessorStage>::is_always_lock_free,
              "ProcessorStage atomic must be lock-free");

class ProcessorSync {
public:
  ProcessorSync() = default;
  ProcessorSync(const ProcessorSync &) = delete;
  ProcessorSync &operator=(const ProcessorSync &) = delete;

  [[nodiscard]] bool start_process_block() noexcept {
    // If prepare/release has requested exclusive access, do not start
    // a new audio block.
    if (blocking_requests.load(std::memory_order_acquire) != 0) {
      return false;
    }

    ProcessorStage expected = ProcessorStage::Free;

    if (!stage.compare_exchange_strong(expected, ProcessorStage::ProcessBlock,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
      return false;
    }

    // Close a small race: if a blocking request arrived just after the
    // check but before we acquired the stage, release immediately.
    if (blocking_requests.load(std::memory_order_acquire) != 0) {
      const bool released = end(ProcessorStage::ProcessBlock);
      jassert(released);
      (void)released;
      return false;
    }

    return true;
  }

  [[nodiscard]] bool start_prepare_to_play(int timeout_ms = 2000) noexcept {
    return enter_blocking(ProcessorStage::PrepareToPlay, timeout_ms);
  }

  [[nodiscard]] bool start_release_resources(int timeout_ms = 2000) noexcept {
    return enter_blocking(ProcessorStage::ReleaseResources, timeout_ms);
  }

  [[nodiscard]] bool end(ProcessorStage expected_stage) noexcept {
    ProcessorStage expected = expected_stage;

    const bool released = stage.compare_exchange_strong(
        expected, ProcessorStage::Free, std::memory_order_release,
        std::memory_order_relaxed);

    // When leaving prepare/release, reduce the blocking request count.
    // Do not change this for ProcessBlock, because a blocking operation
    // may still be waiting.
    if (released && expected_stage != ProcessorStage::ProcessBlock) {
      blocking_requests.fetch_sub(1, std::memory_order_acq_rel);
    }

    return released;
  }

private:
  bool enter_blocking(ProcessorStage new_stage, int timeout_ms) noexcept {
    // Ask the audio thread not to start new processBlock calls.
    blocking_requests.fetch_add(1, std::memory_order_acq_rel);

    const auto start = std::chrono::steady_clock::now();
    const auto timeout =
        std::chrono::milliseconds(timeout_ms < 0 ? 0 : timeout_ms);

    ProcessorStage expected = ProcessorStage::Free;

    while (!stage.compare_exchange_strong(expected, new_stage,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed)) {

      if (std::chrono::steady_clock::now() - start > timeout) {
        // Failed to acquire. Allow processing again.
        blocking_requests.fetch_sub(1, std::memory_order_acq_rel);
        return false;
      }

      std::this_thread::yield();
      expected = ProcessorStage::Free;
    }

    return true;
  }

  std::atomic<ProcessorStage> stage{ProcessorStage::Free};
  std::atomic<int> blocking_requests{0};
};

struct ScopedStage {
  ScopedStage(const ScopedStage &) = delete;
  ScopedStage &operator=(const ScopedStage &) = delete;

  ScopedStage(ProcessorSync &s, ProcessorStage t)
      : sync(s), stage(t), active(true) {}

  ~ScopedStage() {
    if (active) {
      const bool released = sync.end(stage);
      jassert(released);
      (void)released;
    }
  }

  void dismiss() noexcept { active = false; }

  ProcessorSync &sync;
  ProcessorStage stage;
  bool active;
};

enum ProcessorType { InOut, In, Out, MidiOnly };

/// Class for the audio processor which runs the Csound.
// Value of this class can be used to define all methods of the plugin which
// relies on Csound audio processing.
class Processor {
  friend class CsoundLogConsumer;

public:
  /// The processor is initialized with CSD file content, layout of IO-busses,
  /// specification of the parameters and reference to the JUCE audio processor
  /// class.
  Processor(const std::string &csd, const csd_plugin::IOLayout &,
            const ParameterSpec &parameter_spec,
            juce::AudioProcessor &processor);
  ~Processor();

  /// Called on main thread to prepare plugin for audio processing
  void prepareToPlay(double sampleRate, int maxBlockSize);

  /// Called on audio-thread to process audio in blocks
  void processBlock(const juce::AudioProcessor &processor,
                    juce::AudioBuffer<float> &buffer, juce::MidiBuffer &);

  /// Called when audio processing is over to release resources
  void releaseResources();

  /// Latency of the processor in samples.
  // For plugins which process no audio input the latency is zero, and for
  // plugins which do process inputs the latency equals to ksmps.
  int get_latency_samples();

  /// Serialize the parameters of the plugin to JSON
  void getStateInformation(juce::MemoryBlock &destData);

  /// Deserialize the parameters of the plugin from JSON
  void setStateInformation(const void *data, int sizeInBytes);

  /// Get plugin parameters
  Parameters &get_parameters();

  /// Get IO-layout of the plugin
  const csd_plugin::IOLayout &get_io_layout() const;

  /// RT-safe custom logging for the plugin developer.
  // Must pass a C-string (e.g., string literal or .toRawUTF8()) to avoid
  // allocations.
  void log(csd_plugin::LogLevel level, const char *text);

  std::unique_ptr<CsoundLogConsumer> create_log_consumer();

  bool is_csound_valid() const { return csound.is_csound_valid(); }

  std::string get_last_error() const { return csound.get_last_error(); }

  ProcessorSync &get_sync() { return sync; }

private:
  void read_midi_from_host(juce::MidiBuffer &,
                           int64_t block_start_global_sample);
  void write_midi_to_host(juce::MidiBuffer &, int64_t block_start_sample,
                          int block_size);

  void process_in_out(juce::AudioBuffer<float> &buffer);
  void process_no_in_out(juce::AudioBuffer<float> &buffer);
  void process_in_no_out(juce::AudioBuffer<float> &buffer);
  void process_no_in_no_out(juce::AudioBuffer<float> &buffer);
  bool csound_process();
  static ProcessorType
  get_processor_type(const csd_plugin::IOLayout &io_layout);

  bool pop_log(LogMessage &msg);

  csd_plugin::Processor csound;
  Parameters parameters;

  std::array<LogMessage, 1024> log_buffer;
  LockFreeSpscQueue<LogMessage> log_queue;

  ProcessorSync sync;
  ProcessorType processor_type{InOut};
  // std::atomic<bool> lifecycle_error{false};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};

} // namespace juce_csd
