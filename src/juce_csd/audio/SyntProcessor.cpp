
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_csd/audio/SyntProcessor.h>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <memory>
#include <ranges>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

const float WRAP_VOLUME_LIMIT = 5.0f;

namespace juce_csd {

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

SyntProcessor::SyntProcessor(const std::string& _csd_file_content, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor, int buffer_size):
  csound(nullptr), csound_settings(),csd_file_content(_csd_file_content), parameters(processor, parameter_spec), is_ready_to_play(false), audio_buffers(0, buffer_size)
 {}

void SyntProcessor::set_csound_midi_callbacks() {
    #if defined(CS_VERSION) && CS_VERSION >= 7
        csound->SetHostAudioIO();
        csound->SetHostMIDIIO();
    #else
        // Fallback for Csound 6 using the underlying C API directly via GetCsound()
        csoundSetHostImplementedAudioIO(csound->GetCsound(), 1, 0);
        csound->SetHostImplementedMIDIIO(1);
    #endif

    csound->SetHostData(this);
    csound->SetExternalMidiInOpenCallback(&SyntProcessor::midi_device_open);
    csound->SetExternalMidiInCloseCallback(&SyntProcessor::midi_device_close);
    csound->SetExternalMidiOutOpenCallback(&SyntProcessor::midi_device_open);
    csound->SetExternalMidiOutCloseCallback(&SyntProcessor::midi_device_close);
    csound->SetExternalMidiReadCallback(&SyntProcessor::midi_read);
    csound->SetExternalMidiWriteCallback(&SyntProcessor::midi_write);
}

int SyntProcessor::midi_device_open(CSOUND *csound_, void **user_data, const char *devName) {
    auto csound_host_data = csoundGetHostData(csound_);
    *user_data = (void *)csound_host_data;
    return 0;
}

int SyntProcessor::midi_device_close(CSOUND *csound_, void *user_data)
{
    return 0;
}

int SyntProcessor::midi_read(CSOUND* csound, void* userData, unsigned char* buf, int max_size) {
    auto* proc = static_cast<SyntProcessor*>(userData);
    if (!proc) return 0;

    auto& queue = proc->midi_buffer;
    int cycle_end_sample = proc->current_sample_end_sample;

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

// TODO: how to work with MIDI output?
// study:
// https://github.com/gogins-dev/csound-vst3/blob/main/CsoundVST3/Source/PluginProcessor.cpp#L205C1-L216C1
int SyntProcessor::midi_write(CSOUND *csound_, void *userData, const unsigned char *midi_buffer, int midi_buffer_size)
{
 /*
    auto csound_host_data = csoundGetHostData(csound_);
    SyntProcessor *processor = static_cast<SyntProcessor *>(csound_host_data);
    MidiChannelMessage channel_message;
    channel_message.plugin_frame = processor->plugin_frame;
    channel_message.message = juce::MidiMessage(midi_buffer, midi_buffer_size, 0);
    processor->midi_output_fifo.enqueue(channel_message);
*/
    return 0;
}


void SyntProcessor::setup_csound(double sampleRate) {
    DBG("LOAD CSOUND FILE\n");
    csound = std::unique_ptr<Csound>(new Csound());
    std::string options = std::format("-n -d -b0 -+rtmidi=NULL -M0 -sr {} -Q0 -m0", static_cast<int>(sampleRate));
    csound->SetOption(options.c_str());

    set_csound_midi_callbacks();
    csound->CompileCSD(csd_file_content.c_str(), 1);
    csound->Start();
}

void SyntProcessor::prepareToPlay (double sampleRate)
{
    if (!is_ready_to_play) {
        setup_csound(sampleRate);
        clear_buffers();
        csound_settings.prepare(csound.get());
        is_ready_to_play = true;
    }
}

void SyntProcessor::processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& host_midi_buffer)
{
    if (is_ready_to_play) {
        clear_excess_output_channels(processor, buffer);
        read_midi_from_host(host_midi_buffer);
        update_parameters();
        csound_process(buffer);
        write_output_buffer_to_host(buffer);
    }
}

void SyntProcessor::releaseResources() {
    is_ready_to_play = false;
    if (csound != nullptr) {
        csound->Reset();
    }
    clear_buffers();
}

void SyntProcessor::clear_excess_output_channels(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer) {
    int total_num_input_channels = processor.getTotalNumInputChannels();
    int total_num_output_channels = processor.getTotalNumOutputChannels();
    for (const auto channel_to_clear: std::views::iota(total_num_input_channels, total_num_output_channels)) {
        buffer.clear(channel_to_clear, 0, buffer.getNumSamples());
    }
}

void SyntProcessor::write_output_buffer_to_host(juce::AudioBuffer<float>& buffer) {
    float sample{0.0};
    for (auto sample_index: std::ranges::iota_view(0, buffer.getNumSamples())) {
        for (auto channel_index: std::ranges::iota_view(0, buffer.getNumChannels())) {
            audio_buffers.read_output(sample);
            buffer.setSample(channel_index, sample_index, wrap_limiter(csound_settings.inverse_zero_dbfs * sample));
        }
    }
}

void SyntProcessor::read_midi_from_host(juce::MidiBuffer& host_midi_messages) {
    // Note: We filter out SysEx here
    // to guarantee 100% RT-safety (no hidden heap allocations)
    for (const auto metadata : host_midi_messages) {
        auto msg = metadata.getMessage();
        if (!msg.isSysEx()) {
            midi_buffer.push(RawMidiEvent(metadata.samplePosition, msg.getRawData(), msg.getRawDataSize()));
        }
    }
}

void SyntProcessor::update_parameters() {
  parameters.update_on_process(csound.get());
}

void SyntProcessor::csound_process(juce::AudioBuffer<float>& buffer) {
    int buffer_size = buffer.getNumSamples();
    csound_cycle_size = get_csound_cycle_size(buffer_size);
    int samples_processed = 0;

    for (int cycle_index: std::ranges::iota_view(0, csound_cycle_size)) {
        current_sample_end_sample = (cycle_index + 1) * csound_settings.ksmps;
        juce::ignoreUnused(cycle_index);

        csound->PerformKsmps();

        const double* spout = csound->GetSpout();
        for (int index: std::ranges::iota_view(0, static_cast<int>(csound_settings.ksmps))) {
            for (int channel: std::ranges::iota_view(0, csound_settings.out_size)) {
                audio_buffers.write_output(spout[2 * index + channel]);
            }
        }
        samples_processed += csound_settings.ksmps;
    }
}

void SyntProcessor::clear_buffers() {
    audio_buffers.clear();
    midi_buffer.clear();
}

int SyntProcessor::get_csound_cycle_size(int block_size) {
    int stored_buffer_sample_size = audio_buffers.output_size() / csound_settings.out_size;
    if (block_size > stored_buffer_sample_size) {
           return std::ceil(static_cast<double>(block_size - stored_buffer_sample_size) / csound_settings.ksmps);
       } else {
        return 0;
    }
}

void SyntProcessor::getStateInformation (juce::MemoryBlock& destData) {
  parameters.getStateInformation(destData);
}

void SyntProcessor::setStateInformation (const void* data, int sizeInBytes) {
  parameters.setStateInformation(data, sizeInBytes);
}

juce::AudioParameterFloat& SyntProcessor::get_param(const std::string& name) {
    return parameters.get_audio_parameter_ref(name);
}

Parameters& SyntProcessor::get_parameters() {
    return parameters;
}
}
