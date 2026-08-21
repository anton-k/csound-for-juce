
#include <juce_csd/audio/FxProcessor.h>
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

FxProcessor::FxProcessor(const std::string& _csd_file_content, const ParameterSpec& parameter_spec, juce::AudioProcessor& processor):
  csound(nullptr), csound_settings(),csd_file_content(_csd_file_content), parameters(processor, parameter_spec), is_ready_to_play(false)
 {}

int FxProcessor::getLatencySamples() {
    return csound_settings.ksmps;
}

void FxProcessor::prepareToPlay (double sampleRate)
{
    if (!is_ready_to_play) {
        DBG("LOAD CSOUND FILE\n");
        csound = std::unique_ptr<Csound>(new Csound());
        std::string options = std::format("-n -d -b0 -+rtmidi=NULL -M0 -sr {}", static_cast<int>(sampleRate));
        csound->SetOption(options.c_str());

        #if defined(CS_VERSION) && CS_VERSION >= 7
            csound->SetHostAudioIO();
        #else
            // Fallback for Csound 6 using the underlying C API directly via GetCsound()
            csoundSetHostImplementedAudioIO(csound->GetCsound(), 1, 0);
        #endif
        csound->CompileCSD(csd_file_content.c_str(), 1);
        csound->Start();

        audio_buffers.clear();
        csound_settings.prepare(csound.get());
        int frame_size = csound_settings.ksmps * csound_settings.out_size;
        for (int index: std::ranges::iota_view(0, frame_size)) {
            juce::ignoreUnused(index);
            audio_buffers.write_output(0.0);
        }
        is_ready_to_play = true;
    }
}

void FxProcessor::processBlock(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer)
{
    if (is_ready_to_play) {
        clear_excess_output_channels(processor, buffer);
        update_parameters();
        read_input_buffer_from_host(buffer);
        csound_process(buffer);
        write_output_buffer_to_host(buffer);
    }

}

void FxProcessor::releaseResources() {
    is_ready_to_play = false;
    if (csound != nullptr) {
        csound->Reset();
    }
    audio_buffers.clear();
}

void FxProcessor::clear_excess_output_channels(const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer) {
    int total_num_input_channels = processor.getTotalNumInputChannels();
    int total_num_output_channels = processor.getTotalNumOutputChannels();
    for (const auto channel_to_clear: std::views::iota(total_num_input_channels, total_num_output_channels)) {
        buffer.clear(channel_to_clear, 0, buffer.getNumSamples());
    }
}

void FxProcessor::read_input_buffer_from_host(juce::AudioBuffer<float>& buffer) {
    for (auto sample_index: std::ranges::iota_view(0, buffer.getNumSamples())) {
        for (auto channel_index: std::ranges::iota_view(0, buffer.getNumChannels())) {
            audio_buffers.write_input(csound_settings.zero_dbfs * buffer.getSample(channel_index, sample_index));
        }
    }
}

void FxProcessor::write_output_buffer_to_host(juce::AudioBuffer<float>& buffer) {
    float sample{0.0};
    for (auto sample_index: std::ranges::iota_view(0, buffer.getNumSamples())) {
        for (auto channel_index: std::ranges::iota_view(0, buffer.getNumChannels())) {
            audio_buffers.read_output(sample);
            buffer.setSample(channel_index, sample_index, wrap_limiter(csound_settings.inverse_zero_dbfs * sample));
        }
    }
}

void FxProcessor::update_parameters() {
  parameters.update_on_process(csound.get());
}

void FxProcessor::csound_process(juce::AudioBuffer<float>& buffer) {
    int buffer_size = buffer.getNumSamples();
    csound_cycle_size = get_csound_cycle_size(buffer_size);
    float sample{0.0};

    for (int cycle_index: std::ranges::iota_view(0, csound_cycle_size)) {
        juce::ignoreUnused(cycle_index);

        double* spin = csound->GetSpin();
        for (int index: std::ranges::iota_view(0, csound_settings.ksmps)) {
            for (int channel: std::ranges::iota_view(0, csound_settings.in_size)) {
                audio_buffers.read_input(sample);
                spin[2 * index + channel] = static_cast<double>(sample);
            }
        }

        csound->PerformKsmps();

        const double* spout = csound->GetSpout();
        for (int index: std::ranges::iota_view(0, static_cast<int>(csound_settings.ksmps))) {
            for (int channel: std::ranges::iota_view(0, csound_settings.out_size)) {
                audio_buffers.write_output(spout[2 * index + channel]);
            }
        }
    }
}

int FxProcessor::get_csound_cycle_size(int block_size) {
    int stored_buffer_sample_size = audio_buffers.output_size() / csound_settings.out_size;
    if (block_size > stored_buffer_sample_size) {
           return std::ceil(static_cast<double>(block_size - stored_buffer_sample_size) / csound_settings.ksmps);
       } else {
        return 0;
    }
}

void FxProcessor::getStateInformation (juce::MemoryBlock& destData) {
  parameters.getStateInformation(destData);
}

void FxProcessor::setStateInformation (const void* data, int sizeInBytes) {
  parameters.setStateInformation(data, sizeInBytes);
}

juce::AudioParameterFloat& FxProcessor::get_param(const std::string& name) {
    return parameters.get_audio_parameter_ref(name);
}

Parameters& FxProcessor::get_parameters() {
    return parameters;
}
}
