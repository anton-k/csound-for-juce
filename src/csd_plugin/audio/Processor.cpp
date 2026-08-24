
#include "csd_plugin/audio/AudioBuffer.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include <cstdint>
#include <juce_csd/audio/Processor.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <memory>
#include <ranges>
#include <vector>

const float WRAP_VOLUME_LIMIT = 5.0f;

namespace csd_plugin {

namespace {

float wrap_limiter(float sample) {
    if (std::abs(sample) > WRAP_VOLUME_LIMIT) {
        return WRAP_VOLUME_LIMIT * std::signbit(sample);
    } else {
        return sample;
    }
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
    if (io_layout.has_midi_in) {
        csound->SetExternalMidiInOpenCallback(&Processor::midi_device_open);
        csound->SetExternalMidiInCloseCallback(&Processor::midi_device_close);
        csound->SetExternalMidiReadCallback(&Processor::midi_read);
    }

    if (io_layout.has_midi_out) {
        csound->SetExternalMidiOutOpenCallback(&Processor::midi_device_open);
        csound->SetExternalMidiOutCloseCallback(&Processor::midi_device_close);
        csound->SetExternalMidiWriteCallback(&Processor::midi_write);
    }
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
    if (!proc) return 0;

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
            break;
        }
    }

    return bytes_written;
}

int Processor::midi_write(CSOUND *csound_, void *userData, const unsigned char *midi_buffer, int midi_buffer_size)
{
    auto csound_host_data = csoundGetHostData(csound_);
    Processor *processor = static_cast<Processor *>(csound_host_data);
    csd_plugin::RawMidiEvent midi_event{processor->get_current_sample(), midi_buffer, static_cast<uint8_t>(midi_buffer_size)};
    processor->midi_buffers.out().push(midi_event);
    return 0;
}


void Processor::setup_csound(int sample_rate) {
    csound = std::unique_ptr<Csound>(new Csound());
    std::string options = std::format("-n -d -b0 -+rtmidi=NULL -M0 -sr {} -Q0 -m0", static_cast<int>(sample_rate));
    csound->SetOption(options.c_str());
    set_host_io();
    set_csound_midi_callbacks();
    csound->CompileCSD(csd_file_content.c_str(), 1);
    csound->Start();
}

void Processor::prepare_to_play (int sample_rate, int max_block_size)
{
    if (!ready_to_play) {
        setup_csound(sample_rate);
        csound_settings.prepare(csound.get());
        clear_buffers();
        audio_buffers.reset(io_layout.get_in_size() * 2 * max_block_size, io_layout.get_out_size() * 2 * max_block_size);
        if (io_layout.get_in_size() > 0) {
            for (int index: std::ranges::iota_view(0, csound_settings.ksmps)) {
                audio_buffers.out().write(0.0f);
            }
        }

        current_sample = 0;
        ready_to_play = true;
    }
}

void Processor::process_block(int block_size)
{
    if (ready_to_play) {
        csound_process(block_size);
    }
}

void Processor::release_resources() {
    ready_to_play = false;
    if (csound != nullptr) {
        csound->Reset();
    }
    clear_buffers();
    current_sample = 0;
}

void Processor::csound_process(int buffer_size) {
    csound_cycle_size = get_csound_cycle_size(buffer_size);
    int in_size = io_layout.get_in_size();
    int out_size = io_layout.get_out_size();

    float sample{0.0};
    for (int cycle_index: std::ranges::iota_view(0, csound_cycle_size)) {
        current_cycle_end_sample = (cycle_index + 1) * csound_settings.ksmps;

        if (in_size > 0) {
            double* spin = csound->GetSpin();
            for (int index: std::ranges::iota_view(0, csound_settings.ksmps)) {
                for (int channel: std::ranges::iota_view(0, in_size)) {
                    audio_buffers.in().read(sample);
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

        current_sample += csound_settings.ksmps;
    }
}

void Processor::clear_buffers() {
    audio_buffers.clear();
    midi_buffers.clear();
}

int Processor::get_csound_cycle_size(int block_size) {
    int stored_buffer_sample_size = audio_buffers.out().get_size() / csound_settings.out_size;
    if (block_size > stored_buffer_sample_size) {
           return std::ceil(static_cast<double>(block_size - stored_buffer_sample_size) / csound_settings.ksmps);
       } else {
        return 0;
    }
}

int Processor::get_current_sample() {
    return current_sample;
}

}
