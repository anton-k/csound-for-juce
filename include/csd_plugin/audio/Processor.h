#pragma once

#include "AudioBuffer.h"
#include "MidiBuffer.h"
#include "csd_plugin/audio/Logger.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <csound/csound.hpp>
#include <csound/sysdep.h>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace csd_plugin {

inline constexpr MYFLT WRAP_VOLUME_LIMIT = static_cast<MYFLT>(5.0f);

inline MYFLT wrap_limiter(MYFLT sample) {
  if (!std::isfinite(static_cast<double>(sample))) {
    return MYFLT{0};
  }
  return std::clamp(sample, -WRAP_VOLUME_LIMIT, WRAP_VOLUME_LIMIT);
}

/// Settings for Csound file
struct CsoundSettings {
  CsoundSettings();
  void prepare(Csound *);
  void set_channel_names(Csound *);
  int ksmps{1};
  int sample_rate{44100};
  int out_size{2};
  int in_size{0};
  MYFLT zero_dbfs{1.0}, inverse_zero_dbfs{1.0};
  std::vector<std::string> channel_names;
};

/// Layout of the IO-busses. How many inputs/outputs. Does it have MIDI or
/// side-chain.
struct IOLayout {
  IOLayout() = default;
  IOLayout(const IOLayout &) = default;
  IOLayout &operator=(const IOLayout &) = default;

  bool has_midi_in{false};  ///< Has MIDI input
  bool has_midi_out{false}; ///< Has MIDI output

  /// Total size equals to the sum of input and sidechain busses
  int get_total_in_size() const { return in_size + sidechain_size; }

  /// Returns size of the output busses
  int get_out_size() const { return out_size; }

  static IOLayout synth() {
    IOLayout layout{};
    layout.sidechain_size = 0;
    layout.has_midi_in = true;
    layout.has_midi_out = false;
    layout.in_size = 0;
    layout.out_size = 2;
    return layout;
  };

  static IOLayout synth_mono() {
    IOLayout layout{};
    layout.sidechain_size = 0;
    layout.has_midi_in = true;
    layout.has_midi_out = false;
    layout.in_size = 0;
    layout.out_size = 1;
    return layout;
  };

  static IOLayout fx() {
    IOLayout layout{};
    layout.sidechain_size = 0;
    layout.has_midi_in = false;
    layout.has_midi_out = false;
    layout.in_size = 2;
    layout.out_size = 2;
    return layout;
  };

  static IOLayout fx_mono() {
    IOLayout layout{};
    layout.sidechain_size = 0;
    layout.has_midi_in = false;
    layout.has_midi_out = false;
    layout.in_size = 1;
    layout.out_size = 1;
    return layout;
  };

  IOLayout with_sidechain(int size) const {
    IOLayout layout(*this);
    layout.sidechain_size = size;
    return layout;
  }

  int in_size{2};        ///< how many inputs (stereo is 2)
  int out_size{2};       ///< how many outputs (stereo is 2)
  int sidechain_size{0}; ///< size of the side-chain  inputs
  int extra_latency_samples{
      0}; ///< constant extra latency in samples of the plugin
};

class CsdInputAudioBuffer {
public:
  CsdInputAudioBuffer() = default;
  CsdInputAudioBuffer(int size_, MYFLT scale_, int channel_size_)
      : capacity(size_), scale(scale_), channel_size(channel_size_) {
    if (capacity > 0) {
      // Allocate extra space to accommodate block-size mismatches
      buffer.reset(capacity * 2);
    }
  }

  bool is_valid() const { return capacity > 0; }
  int get_capacity() const { return capacity; }
  bool is_full() const { return buffer.get_size() >= capacity; }

  int get_free_frames() const {
    if (channel_size == 0)
      return 0;
    return buffer.get_free_space() / channel_size;
  }

  void feed_csound_spin(MYFLT *spin, int ksmps) {
    if (spin == nullptr || capacity <= 0)
      return;
    int total_samples = ksmps * channel_size;
    int read_count = buffer.read_block_partial(spin, total_samples);
    for (int i = read_count; i < total_samples; ++i) {
      spin[i] = 0.0;
    }
  }

  bool write(MYFLT sample) {
    if (capacity <= 0)
      return false;
    return buffer.write(sample * scale);
  }

  void clear() { buffer.clear(); }

private:
  AudioBuffer<MYFLT> buffer;
  int capacity{0};
  MYFLT scale{1.0};
  int channel_size{0};
};

class CsdOutputAudioBuffer {
public:
  CsdOutputAudioBuffer() = default;
  CsdOutputAudioBuffer(int size_, MYFLT scale_, int channel_size_)
      : capacity(size_), scale(scale_), channel_size(channel_size_) {
    if (capacity > 0) {
      // Allocate extra space to accommodate prefill and block-size mismatches
      buffer.reset(capacity * 2);
    }
  }

  int get_size() const { return buffer.get_size(); }
  bool is_valid() const { return capacity > 0; }

  void collect_csound_spout(const MYFLT *spout, int ksmps) {
    if (spout == nullptr || capacity <= 0)
      return;
    int total_samples = ksmps * channel_size;
    for (int i = 0; i < total_samples; ++i) {
      buffer.write(wrap_limiter(spout[i] * scale));
    }
  }

  bool read(MYFLT &sample) {
    if (capacity <= 0) {
      sample = 0.0;
      return false;
    }
    bool ok = buffer.read(sample);
    if (!ok) {
      sample = 0.0;
    }
    return ok;
  }

  int available_frames() const {
    if (channel_size == 0)
      return 0;
    return buffer.get_size() / channel_size;
  }

  void prefill_zeroes(int num_samples) {
    for (int i = 0; i < num_samples; ++i) {
      buffer.write(0.0);
    }
  }

  void clear() { buffer.clear(); }

private:
  AudioBuffer<MYFLT> buffer;
  int capacity{0};
  MYFLT scale{1.0};
  int channel_size{0};
};

class CsdAudioBuffers {
public:
  CsdAudioBuffers() = default;
  CsdAudioBuffers(const CsoundSettings &settings, const IOLayout &io_layout)
      : input_buffer(settings.ksmps * io_layout.get_total_in_size(),
                     settings.zero_dbfs, io_layout.get_total_in_size()),
        output_buffer(settings.ksmps * io_layout.get_out_size(),
                      settings.inverse_zero_dbfs, io_layout.get_out_size()),
        has_input(io_layout.get_total_in_size() > 0),
        has_output(io_layout.get_out_size() > 0), initialized(true) {
    // Prefill output buffer with zeroes for FX (input && output) to compensate
    // latency
    if (has_input && has_output) {
      int prefill_samples = settings.ksmps * io_layout.get_out_size();
      output_buffer.prefill_zeroes(prefill_samples);
    }
  }

  bool is_valid() const {
    return initialized && (!has_input || input_buffer.is_valid()) &&
           (!has_output || output_buffer.is_valid());
  }

  bool is_full() const { return input_buffer.is_full(); }

  bool write(MYFLT sample) { return input_buffer.write(sample); }

  bool read(MYFLT &sample) { return output_buffer.read(sample); }

  void clear() {
    input_buffer.clear();
    output_buffer.clear();
  }

  int available_output_frames() const {
    return output_buffer.available_frames();
  }

  int get_free_frames() const { return input_buffer.get_free_frames(); }

  void prepare_for_csound(Csound *csound, int ksmps) {
    if (has_input) {
      input_buffer.feed_csound_spin(csound->GetSpin(), ksmps);
    }
  }

  void collect_from_csound(Csound *csound, int ksmps) {
    if (has_output) {
      output_buffer.collect_csound_spout(csound->GetSpout(), ksmps);
    }
  }

  void csound_performed() {}

  void reset() {}

private:
  CsdInputAudioBuffer input_buffer;
  CsdOutputAudioBuffer output_buffer;
  bool has_input{false};
  bool has_output{false};
  bool initialized{false};
};

class Timer {
public:
  Timer() = default;

  void next(int64_t steps) { current_sample += steps; }

  void reset() { current_sample = 0; }

  int64_t get_current_sample() const { return current_sample; }

private:
  int64_t current_sample{0};
};

struct CsdLog {
  // Called by UI thread. Allocates a std::string for JUCE, which is safe.
  std::string get_last_error() const {
    if (!has_error_.load(std::memory_order_acquire)) {
      return "";
    }

    char local[sizeof(last_error_buffer_)];
    uint32_t seq = 0;

    for (;;) {
      seq = error_seq.load(std::memory_order_acquire);

      // An odd sequence number means a writer is currently updating the buffer.
      if ((seq & 1u) != 0) {
        continue;
      }

      std::memcpy(local, last_error_buffer_, sizeof(local));

      if (error_seq.load(std::memory_order_acquire) == seq) {
        break;
      }
    }

    size_t len = 0;
    while (len < sizeof(local) && local[len] != '\0') {
      ++len;
    }

    return std::string(local, len);
  }

  void set_last_error(const char *msg) {
    if (!msg)
      return;

    // Enter write state (odd sequence number).
    error_seq.fetch_add(1u, std::memory_order_acq_rel);

    size_t i = 0;
    // Manual bounded copy to avoid strncpy's zero-padding overhead
    for (; i < sizeof(last_error_buffer_) - 1 && msg[i] != '\0'; ++i) {
      last_error_buffer_[i] = msg[i];
    }
    last_error_buffer_[i] = '\0';

    // Leave write state (even sequence number).
    error_seq.fetch_add(1u, std::memory_order_acq_rel);

    has_error_.store(true, std::memory_order_release);
  }

  void clear_last_error() {
    has_error_.store(false, std::memory_order_release);
  }

  LogCallback log_callback{nullptr};
  std::atomic<bool> is_compiling{false};
  std::string compilation_log_buffer;
  char last_error_buffer_[4096] = {0};
  std::atomic<bool> has_error_{false};
  std::atomic<uint32_t> error_seq{0};
};

/// Defines audio processing with Csound.
// for real-time safety in you csd files:
//
// - avoid file loading in instruments;
// - avoid `printf`/`printk` in production;
// - avoid opcodes with unbounded initialization cost;
// - avoid dynamic score generation in the audio thread unless carefully
// controlled.
class Processor {
public:
  /// Constructs processor with the content of CSD-file, layout of the IO-busses
  Processor(const std::string &csd, const IOLayout &io_layout_)
      : csound(nullptr), csd_file_content(csd), io_layout(io_layout_) {}

  void shutdown();

  ~Processor() { shutdown(); }

  /// Called prior to audio processing. It compiles the CSD-file and
  /// instantiates
  // the buffers and reads all constants from Csoun dsettings that are needed
  // for audio processing
  bool prepare_to_play(int sample_rate);

  /// Releases resources. Called after audio processing has stopped
  void release_resources();

  /// Reads raw pointer to the Csound API
  Csound *get_csound() { return csound.get(); };

  /// Reads latency in samples. If processor has no inputs latency is zero,
  // of it has inputs it equals to the ksmps.
  int get_latency_samples();

  // TODO: define the same read/write functions for MIDI-buffers

  /// Writes sample to the input audio buffer. Use it prior to process_block to
  /// read
  // all samples from the host
  void write_input(MYFLT sample);

  /// Reads sample from the output audio buffer. Use it after process_block to
  /// read
  // read sampels processed with Csound and write them to the host.
  bool read_output(MYFLT &sample);

  /// Is Csound ready to play
  bool is_ready_to_play() const { return ready_to_play; };

  /// Returns IO-layout of the processr
  const IOLayout &get_io_layout() const { return io_layout; }

  /// Returns audio buffers
  CsdAudioBuffers &get_audio_buffers() { return audio_buffers; }

  /// Returns midi buffers
  MidiBuffers &get_midi_buffers() { return midi_buffers; }

  /// Reads Csound settings
  CsoundSettings &get_csound_settings() { return csound_settings; }

  /// Returns absolute processing time in samples (How many samples were
  /// processed so far from the call to prepare_to_play)
  int64_t get_current_sample() const { return timer.get_current_sample(); }

  int64_t get_cycle_end_sample() const {
    return timer.get_current_sample() + csound_settings.ksmps;
  }

  void set_log_callback(LogCallback callback) {
    logger.log_callback = std::move(callback);
  }

  void log(LogLevel level, const char *str) {
    if (level == LogLevel::Error) {
      logger.set_last_error(str);
    }

    if (logger.log_callback != nullptr) {
      logger.log_callback(level, str);
    }
  }

  bool is_csound_valid() const {
    return ready_to_play.load(std::memory_order_acquire);
  }

  std::string get_last_error() const { return logger.get_last_error(); }

  /// One cycle of Csound audio processing. It will perform processing
  /// only if csound was successfully initialized in prepare_to_play.
  bool process();

  /// Returns how many cycles Csound should run to produce audio for given
  /// block_size.
  int get_csound_cycle_size(int block_size);

private:
  bool validate_io_layout();

  void prepare_audio_buffers();
  bool prepare_csound_to_play(int sample_rate);

  void set_csound_midi_callbacks();
  void set_host_io();
  bool setup_csound(int sample_rate);
  void clear_buffers();
  void stop_and_reset_csound();

  // midi callbacks
  static int midi_read(CSOUND *, void *userData, unsigned char *buf, int n);
  static int midi_write(CSOUND *csound_, void *userData,
                        const unsigned char *midi_buffer, int midi_buffer_size);
  static int midi_device_open(CSOUND *csound_, void **user_data,
                              const char *devName);
  static int midi_device_close(CSOUND *csound_, void *user_data);

  // logger callback
  static void csound_message_callback(CSOUND *csound, int attr,
                                      const char *format, va_list val);

  std::unique_ptr<Csound> csound;
  std::string csd_file_content;
  IOLayout io_layout;
  CsoundSettings csound_settings{};
  CsdAudioBuffers audio_buffers{};
  MidiBuffers midi_buffers{1024, 1024};
  std::atomic<bool> ready_to_play{false};
  std::atomic<bool> needs_full_reinit{true};
  Timer timer{};
  CsdLog logger;
};

} // namespace csd_plugin
