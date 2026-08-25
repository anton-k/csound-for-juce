
#include "csd_plugin/audio/AudioBuffer.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include <cstdint>
#include <juce_csd/audio/Processor.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <ranges>

const float WRAP_VOLUME_LIMIT = 5.0f;

namespace csd_plugin {

namespace {

float wrap_limiter(float sample) {
    return std::clamp(sample, -WRAP_VOLUME_LIMIT, WRAP_VOLUME_LIMIT);
}


bool is_control_channel_type(controlChannelInfo_t info) {
  return (info.type & CSOUND_CHANNEL_TYPE_MASK) == CSOUND_CONTROL_CHANNEL;
}

}


CsoundSettings::CsoundSettings(): ksmps(1), out_size(2), in_size(0) {}

void CsoundSettings::prepare(Csound* csound) {
    ksmps = static_cast<size_t>(csound->GetKsmps());
    out_size = static_cast<size_t>(csound->GetChannels(0));
    in_size = static_cast<size_t>(csound->GetChannels(1));
    sample_rate = csound->GetSr();
    zero_dbfs = csound->Get0dBFS();
    if (zero_dbfs < 0.01) {
        zero_dbfs = 1.f;
        inverse_zero_dbfs = 1.f;
    } else {
        inverse_zero_dbfs = 1.f / zero_dbfs;
    }
    set_channel_names(csound);
}

void CsoundSettings::set_channel_names(Csound* csound) {
  controlChannelInfo_t* channel_list = nullptr;
  channel_names.clear();
  int num_channels = csound->ListChannels(channel_list);
  if (num_channels > 0 && channel_list != nullptr) {
    for (int32_t i = 0; i < num_channels; ++i) {
      if (is_control_channel_type(channel_list[i])) {
        channel_names.push_back(channel_list[i].name);
      }
    }
  }
  csound->DeleteChannelList(channel_list);
}

int Processor::get_latency_samples() {
    if (io_layout.get_total_in_size() > 0) {
        return csound_settings.ksmps;
    } else {
        return 0;
    }
}

void Processor::write_input(float sample) {
    audio_buffers.in().write(csound_settings.zero_dbfs * sample);
}

void Processor::read_output(float& sample) {
    audio_buffers.out().read(sample);
    sample = wrap_limiter(csound_settings.inverse_zero_dbfs * sample);
}

void Processor::set_host_io() {
    #if defined(CS_VERSION) && CS_VERSION >= 7
        csound->SetHostAudioIO();
        csound->SetHostMIDIIO();
    #else
        // Fallback for Csound 6 using the underlying C API directly via GetCsound()
        csoundSetHostImplementedAudioIO(csound->GetCsound(), 1, 0);
        csound->SetHostImplementedMIDIIO(1);
    #endif
}

void Processor::set_csound_midi_callbacks() {
    csound->SetHostData(this);
    csound->SetExternalMidiInOpenCallback(&Processor::midi_device_open);
    csound->SetExternalMidiInCloseCallback(&Processor::midi_device_close);
    csound->SetExternalMidiReadCallback(&Processor::midi_read);

    csound->SetExternalMidiOutOpenCallback(&Processor::midi_device_open);
    csound->SetExternalMidiOutCloseCallback(&Processor::midi_device_close);
    csound->SetExternalMidiWriteCallback(&Processor::midi_write);
}

int Processor::midi_device_open(CSOUND *csound_, void **user_data, const char *devName) {
    auto csound_host_data = csoundGetHostData(csound_);
    *user_data = (void *)csound_host_data;
    return 0;
}

int Processor::midi_device_close(CSOUND *csound_, void *user_data)
{
    return 0;
}

int Processor::midi_read(CSOUND* csound, void* userData, unsigned char* buf, int max_size) {
    auto* proc = static_cast<Processor*>(userData);
    if (!proc || !(proc->get_io_layout().has_midi_in)) return 0;

    auto& queue = proc->midi_buffers.in();
    int cycle_end_sample = proc->current_cycle_end_sample;

    int bytes_written = 0;
    RawMidiEvent next_event;

    while (queue.peek(next_event)) {
        int msg_size = next_event.size;

        if (next_event.samplePosition < cycle_end_sample) {
            if (bytes_written + msg_size > max_size) {
                break;
            }

            queue.pop();
            std::memcpy(buf + bytes_written, next_event.data, msg_size);
            bytes_written += msg_size;
        } else {
            break; // Event is for the future cycle
        }
    }

    return bytes_written;
}

int Processor::midi_write(CSOUND *csound_, void *userData, const unsigned char *midi_buffer, int midi_buffer_size)
{
    auto csound_host_data = csoundGetHostData(csound_);
    Processor *processor = static_cast<Processor *>(csound_host_data);
    if (!processor || !(processor->get_io_layout().has_midi_out)) return 0;

    csd_plugin::RawMidiEvent midi_event{processor->get_current_sample(), midi_buffer, static_cast<uint8_t>(midi_buffer_size)};
    processor->midi_buffers.out().push(midi_event);
    return 0;
}


void Processor::setup_csound(int sample_rate) {
    csound = std::unique_ptr<Csound>(new Csound());

    set_host_io();
    set_csound_midi_callbacks();
    std::string options = std::format("-n -d -b0 -+rtmidi=NULL -M0 -sr {} -Q0 -m0", static_cast<int>(sample_rate));
    csound->SetOption(options.c_str());    csound->CompileCSD(csd_file_content.c_str(), 1);
    csound->Start();
}

void Processor::prepare_to_play(int sample_rate, int max_block_size) {
    bool needs_reinit = !ready_to_play || (current_sample_rate != sample_rate);
    bool needs_buffer_resize = !ready_to_play || (current_max_block_size != max_block_size);

    // 1. ALWAYS reset playback state
    current_sample = 0;
    clear_buffers();

    // 2. Safely re-initialize Csound if sample rate changed
    if (needs_reinit) {
        // WAIT for the audio thread to finish its current block.
        // This blocks the MESSAGE/SETUP thread, which is perfectly safe.
        // The AUDIO thread is NEVER blocked.
        while (is_processing_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // Now it is 100% guaranteed that process_block is not running.
        if (csound != nullptr) {
            csound.reset();    // Safe to destroy
        }

        setup_csound(sample_rate);
        csound_settings.prepare(csound.get());

        current_sample_rate = sample_rate;
        ready_to_play = true;
    }

    // 3. Resize buffers if needed
    if (needs_buffer_resize || needs_reinit) {
        int max_frames = max_block_size + (csound_settings.ksmps * 2);
        int in_capacity = std::max(1024, max_frames * io_layout.get_total_in_size());
        int out_capacity = std::max(1024, max_frames * io_layout.get_out_size());

        audio_buffers.reset(in_capacity, out_capacity);
        current_max_block_size = max_block_size;
    }


    if (audio_buffers.out().get_size() == 0) {
        if (io_layout.get_total_in_size() > 0) {
            for (int index: std::ranges::iota_view(0, csound_settings.ksmps)) {
                audio_buffers.out().write(0.0f);
            }
        }
    }

}


void Processor::process_block(int block_size)
{
    if (ready_to_play) {
        is_processing_.store(true, std::memory_order_release);
        csound_process(block_size);
        is_processing_.store(false, std::memory_order_release);
    }
}

void Processor::release_resources() {
    while (is_processing_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    ready_to_play = false;
    if (csound != nullptr) {
        csound->Reset();
    }
    clear_buffers();
    current_sample = 0;
    current_sample_rate = 0;
    current_max_block_size = 0;
}

void Processor::csound_process(int buffer_size) {
    csound_cycle_size = get_csound_cycle_size(buffer_size);
    int in_size = io_layout.get_total_in_size();
    int out_size = io_layout.get_out_size();

    float sample{0.0};
    for (int cycle_index: std::ranges::iota_view(0, csound_cycle_size)) {
        current_cycle_end_sample = current_sample + csound_settings.ksmps;

        if (in_size > 0) {
            double* spin = csound->GetSpin();
            for (int index: std::ranges::iota_view(0, csound_settings.ksmps)) {
                for (int channel: std::ranges::iota_view(0, in_size)) {
                    if (!audio_buffers.in().read(sample)) {
                        sample = 0.0f;
                    }
                    spin[in_size * index + channel] = static_cast<double>(sample);
                }
            }
        }

        csound->PerformKsmps();

        const double* spout = csound->GetSpout();
        for (int index: std::ranges::iota_view(0, static_cast<int>(csound_settings.ksmps))) {
            for (int channel: std::ranges::iota_view(0, out_size)) {
                audio_buffers.out().write(spout[out_size * index + channel]);
            }
        }
        current_sample = current_cycle_end_sample;
    }
}

void Processor::clear_buffers() {
    audio_buffers.clear();
    midi_buffers.clear();
}

int Processor::get_csound_cycle_size(int block_size) {
    int out_size = io_layout.get_out_size();
    int current_out_frames = audio_buffers.out().get_size() / out_size;

    // Ensure we have enough frames for the host to read, plus 1 ksmps safety margin
    int target_frames = block_size + csound_settings.ksmps;

    if (current_out_frames >= target_frames) {
        return 0;
    }

    int frames_needed = target_frames - current_out_frames;
    int cycles_needed = (frames_needed + csound_settings.ksmps - 1) / csound_settings.ksmps;

    return cycles_needed;
}

int Processor::get_current_sample() {
    return current_sample;
}

}
