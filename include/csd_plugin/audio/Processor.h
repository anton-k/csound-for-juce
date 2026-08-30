#pragma once

#include <csound/csound.hpp>
#include <memory>
#include <sys/types.h>
#include <vector>
#include "AudioBuffer.h"
#include "MidiBuffer.h"

namespace csd_plugin {

/// Settings for Csound file
struct CsoundSettings {
    CsoundSettings();
    void prepare(Csound*);
    void set_channel_names(Csound*);
    int ksmps{1};
    int sample_rate{44100};
    int out_size{2};
    int in_size{0};
    float zero_dbfs{1.0}, inverse_zero_dbfs{1.0};
    std::vector<std::string> channel_names;
};

/// Layout of the IO-busses. How many inputs/outputs. Does it have MIDI or side-chain.
class IOLayout {
  public:
    IOLayout() {};
    IOLayout(const IOLayout& that) {
      sidechain_size = that.sidechain_size;
      has_midi_in = that.has_midi_in;
      has_midi_out = that.has_midi_out;
      in_size = that.in_size;
      out_size = that.out_size;
    }

    bool has_midi_in {false}; ///< Has MIDI input
    bool has_midi_out {false}; ///< Has MIDI output

    /// Total size equals to the sum of input and sidechain busses
    int get_total_in_size() const {
      return in_size + sidechain_size;
    }

    /// Returns size of the output busses
    int get_out_size() const {
      return out_size;
    }

    static IOLayout synt_layout() {
      IOLayout layout{};
      layout.sidechain_size = 0;
      layout.has_midi_in = true;
      layout.has_midi_out = false;
      layout.in_size = 0;
      layout.out_size = 2;
      return layout;
    };

    static IOLayout synt_mono_layout() {
      IOLayout layout{};
      layout.sidechain_size = 0;
      layout.has_midi_in = true;
      layout.has_midi_out = false;
      layout.in_size = 0;
      layout.out_size = 1;
      return layout;
    };

    static IOLayout fx_layout() {
      IOLayout layout{};
      layout.sidechain_size = 0;
      layout.has_midi_in = false;
      layout.has_midi_out = false;
      layout.in_size = 2;
      layout.out_size = 2;
      return layout;
    };

    static IOLayout fx_mono_layout() {
      IOLayout layout{};
      layout.sidechain_size = 0;
      layout.has_midi_in = false;
      layout.has_midi_out = false;
      layout.in_size = 1;
      layout.out_size = 1;
      return layout;
    };


    IOLayout with_sidechain(int size) {
      IOLayout layout(*this);
      layout.sidechain_size = size;
      return layout;
    }

    int in_size {2}; ///< how many inputs (stereo is 2)
    int out_size {2}; ///< how many outputs (stereo is 2)
    int sidechain_size {0}; ///< size of the side-chain  inputs
};

/// Defines audio processing with Csound
class Processor {
  public:
    /// Constructs processor with the content of CSD-file, layout of the IO-busses
    Processor(const std::string& csd, const IOLayout& io_layout_):
      csound(nullptr),
      csd_file_content(csd),
      io_layout(io_layout_) {};
    ~Processor() {
      release_resources();
      csound.reset();
    };

    /// Called prior to audio processing. It compiles the CSD-file and instantiates
    // the buffers and reads all constants from Csoun dsettings that are needed for audio processing
    void prepare_to_play(int sample_rate, int max_block_size);

    /// Process the audio with given block size. It's assumed that when it is
    // called audio samples from the DAW/Host are already written in the input audio buffer
    // as well as MIDI events are written to the MIDI input buffer. After processing
    // the appllication can read processed samples form the output audio buffer and
    // MIDI events from the output MIDI buffer.
    void process_block(int block_size);

    /// Releases resources. Called after audio processing has stopped
    void release_resources();

    /// Reads raw pointer to the Csound API
    Csound* get_csound() {
      return csound.get();
    };

    /// Reads latency in samples. If processor has no inputs latency is zero,
    // of it has inputs it equals to the ksmps.
    int get_latency_samples();

    // TODO: define the same read/write functions for MIDI-buffers

    /// Writes sample to the input audio buffer. Use it prior to process_block to read
    // all samples from the host
    void write_input(float sample);

    /// Reads sample from the output audio buffer. Use it after process_block to read
    // read sampels processed with Csound and write them to the host.
    void read_output(float& sample);

    /// Is Csound ready to play
    bool is_ready_to_play() {
      return ready_to_play;
    };

    /// Returns IO-layout of the processr
    const IOLayout& get_io_layout() const {
      return io_layout;
    }

    /// Returns audio buffers
    AudioBuffers& get_audio_buffers() {
      return audio_buffers;
    }

    /// Returns midi buffers
    MidiBuffers& get_midi_buffers() {
      return midi_buffers;
    }

    /// Reads Csound settings
    CsoundSettings& get_csound_settings() {
      return csound_settings;
    }

    /// Returns absolute processing time in samples (How many samples were processed so far from the call to prepare_to_play)
    int get_current_sample();

  private:
    void csound_process(int block_size);
    int get_csound_cycle_size(int block_size);
    void set_csound_midi_callbacks();
    void set_host_io();
    void setup_csound(int sample_rate);
    void clear_buffers();

    // midi callbacks
    static int midi_read(CSOUND*, void* userData, unsigned char* buf, int n);
    static int midi_write(CSOUND *csound_, void *userData, const unsigned char *midi_buffer, int midi_buffer_size);
    static int midi_device_open(CSOUND *csound_, void **user_data, const char *devName);
    static int midi_device_close(CSOUND *csound_, void *user_data);

    std::unique_ptr<Csound> csound;
    CsoundSettings csound_settings{};
    AudioBuffers audio_buffers{0};
    MidiBuffers midi_buffers{1024, 1024};
    int csound_cycle_size{0};
    int current_sample{0};
    std::string csd_file_content;
    bool ready_to_play{false};
    IOLayout io_layout;
    int current_cycle_end_sample{0};

    std::atomic<bool> is_processing_{false};
    int current_sample_rate{0};
    int current_max_block_size{0};
};

}
