#pragma once

#include <csound/csound.hpp>
#include <memory>
#include <sys/types.h>
#include <vector>
#include "AudioBuffer.h"
#include "MidiBuffer.h"

namespace csd_plugin {

struct CsoundSettings {
    CsoundSettings();
    void prepare(Csound*);
    void set_channel_names(Csound*);
    int ksmps{1};
    int out_size{2};
    int in_size{0};
    float zero_dbfs{1.0}, inverse_zero_dbfs{1.0};
    std::vector<std::string> channel_names;
};

class IOLayout {
  public:
    bool has_sidechain;
    bool has_midi_in;
    bool has_midi_out;

    int get_in_size() {
      return in_size + (has_sidechain ? 2 : 0);
    }

    int get_out_size() {
      return out_size;
    }

  private:
    int in_size;
    int out_size;
};

class Processor {
  public:
    Processor(const std::string& csd, IOLayout& io_layout_):
      csound(nullptr),
      csd_file_content(csd),
      io_layout(io_layout_) {};
    ~Processor() { };

    void prepare_to_play(int sample_rate, int max_block_size);
    void process_block(int block_size);
    void release_resources();
    std::unique_ptr<Csound> get_csound();

    int getLatencySamples();

  private:
    void csound_process(int block_size);
    int get_csound_cycle_size(int block_size);
    void set_csound_midi_callbacks();
    void set_host_io();
    void setup_csound(int sample_rate);
    void clear_buffers();
    int get_current_sample();

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
    bool is_ready_to_play{false};
    IOLayout io_layout;
    int current_cycle_end_sample{0};
};

}
