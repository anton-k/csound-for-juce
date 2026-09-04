#include "csd_plugin/audio/AudioBuffer.h"
#include "csd_plugin/audio/MidiBuffer.h"
#include <csound/sysdep.h>
#include <cstdint>
#include <csound/csound.h>
#include <csound/csound.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <format>
#include <csd_plugin/audio/Logger.h>
#include <csd_plugin/audio/Processor.h>
#include <thread>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <limits>

namespace csd_plugin {

namespace {

inline MYFLT wrap_limiter(MYFLT sample)
{
    if (!std::isfinite(static_cast<double>(sample)))
    {
        return MYFLT{0};
    }

    return std::clamp(
        sample,
        -WRAP_VOLUME_LIMIT,
        WRAP_VOLUME_LIMIT
    );
}

bool is_control_channel_type(controlChannelInfo_t info) {
  return (info.type & CSOUND_CHANNEL_TYPE_MASK) == CSOUND_CONTROL_CHANNEL;
}

constexpr int MIN_FIFO_BLOCK_FRAMES = 8000;
constexpr unsigned int MAX_COMPILATION_LOG_CHARS = 65536;

int get_desired_audio_buffer_size(int channels, int max_block_size, int ksmps)
{
    if (channels <= 0)
        return 0;

    // Some hosts can behave oddly. Keep a conservative hard floor.
    const int64_t safe_max_block = std::max<int64_t>(static_cast<int64_t>(max_block_size), 1);
    const int64_t safe_block_size = std::max<int64_t>(safe_max_block * 2, MIN_FIFO_BLOCK_FRAMES);

    // Worst-case output occupancy should stay below block_size + ksmps.
    // Use extra safety margin.
    const int64_t safety_frames = std::max<int64_t>(static_cast<int64_t>(4) * static_cast<int64_t>(ksmps), 256);

    const int64_t frames = safe_block_size + safety_frames;
    const int64_t samples = frames * static_cast<int64_t>(channels);
    const int64_t max_int = static_cast<int64_t>(std::numeric_limits<int>::max());

    return static_cast<int>(samples > max_int ? max_int : samples);
}

struct ScopedProcessingScope {
    Processor& proc;

    explicit ScopedProcessingScope(Processor& p)
        : proc(p)
    {
        proc.begin_processing_scope();
    }

    ~ScopedProcessingScope()
    {
        proc.end_processing_scope();
    }
};

}

CsoundSettings::CsoundSettings(): ksmps(1), out_size(2), in_size(0) {}

void CsoundSettings::prepare(Csound* csound) {
    ksmps = static_cast<int>(csound->GetKsmps());

    // Do not query audio channel counts here.
    //
    // The channel-count query API is unreliable across Csound versions and host states,
    // especially during sample-rate switches and repeated prepareToPlay() calls.
    // The plugin's declared IOLayout is authoritative for channel counts.

    sample_rate = static_cast<int>(csound->GetSr());
    zero_dbfs = static_cast<MYFLT>(csound->Get0dBFS());

    if (zero_dbfs < static_cast<MYFLT>(0.01)) {
        zero_dbfs = static_cast<MYFLT>(1.f);
        inverse_zero_dbfs = static_cast<MYFLT>(1.f);
    } else {
        inverse_zero_dbfs = static_cast<MYFLT>(1.f) / zero_dbfs;
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
    int csound_processing_latency =
        (io_layout.get_total_in_size() > 0)
            ? csound_settings.ksmps
            : 0;

    return io_layout.extra_latency_samples + csound_processing_latency;
}

void Processor::write_input(MYFLT sample) {
    audio_buffers.in().write(csound_settings.zero_dbfs * sample);
}

bool Processor::read_output(MYFLT& sample) {
    bool read_success = audio_buffers.out().read(sample);

    if (!read_success) {
        sample = 0.0; // Prevent stale memory/feedback loops on underflow
    } else {
        sample = wrap_limiter(csound_settings.inverse_zero_dbfs * sample);
    }

    return read_success;
}

void Processor::set_host_io() {
    #if defined(CS_VERSION) && CS_VERSION >= 7
        csound->SetHostAudioIO();
        csound->SetHostMIDIIO();
    #else
        const int play =
            (io_layout.get_out_size() > 0)
                ? 1
                : 0;

        const int rec =
            (io_layout.get_total_in_size() > 0)
                ? 1
                : 0;

        csoundSetHostImplementedAudioIO(
            csound->GetCsound(),
            play,
            rec
        );

        const int host_midi_io =
            (io_layout.has_midi_in || io_layout.has_midi_out)
                ? 1
                : 0;

        csoundSetHostImplementedMIDIIO(
            csound->GetCsound(),
            host_midi_io
        );
    #endif
}

void Processor::set_csound_midi_callbacks() {
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
    void* host_data = (userData != nullptr) ? userData : csoundGetHostData(csound);
    auto* proc = static_cast<Processor*>(host_data);

    if (!proc || !(proc->get_io_layout().has_midi_in) || max_size <= 0 || buf == nullptr)
        return 0;

    auto& queue = proc->midi_buffers.in();

    const int64_t cycle_end_sample = proc->current_cycle_end_sample;

    int bytes_written = 0;
    RawMidiEvent next_event;

    while (queue.peek(next_event)) {
        const int msg_size = next_event.size;

        if (msg_size == 0) {
            queue.pop();
            continue;
        }

        // Avoid getting stuck forever on an event that cannot fit into Csound's buffer.
        if (msg_size > max_size) {
            queue.pop();
            continue;
        }

        if (next_event.samplePosition < cycle_end_sample) {
            if (bytes_written + msg_size > max_size) {
                break;
            }

            queue.pop();

            std::memcpy(buf + bytes_written, next_event.data, msg_size);
            bytes_written += msg_size;
        } else {
            // Event belongs to a future Csound cycle.
            break;
        }
    }

    return bytes_written;
}

int Processor::midi_write(CSOUND *csound_, void *userData, const unsigned char *midi_buffer, int midi_buffer_size)
{
    void* host_data = (userData != nullptr) ? userData : csoundGetHostData(csound_);
    Processor *processor = static_cast<Processor *>(host_data);

    if (!processor || !(processor->get_io_layout().has_midi_out))
        return 0;

    if (midi_buffer_size <= 0 || midi_buffer == nullptr)
        return 0;

    const int safe_int_size = std::min(midi_buffer_size, MIDI_DATA_SIZE);
    const uint8_t safe_size = static_cast<uint8_t>(safe_int_size);

    csd_plugin::RawMidiEvent midi_event{
        processor->current_cycle_start_sample,
        midi_buffer,
        safe_size
    };

    processor->midi_buffers.out().push(midi_event);

    return 0;
}

void Processor::stop_and_reset_csound() {
    if (csound == nullptr) {
        return;
    }

    // Detach host data first so Csound callbacks cannot find this Processor
    // while we are shutting the engine down.
    csound->SetHostData(nullptr);

    // Destroy the Csound instance completely. The unique_ptr destructor
    // handles all internal Csound cleanup safely. This is the most reliable
    // way to handle sample rate changes, as csound->Reset() does not clear
    // compiled instruments or options.
    csound.reset();
}

bool Processor::setup_csound(int sample_rate) {
    const int safe_sample_rate = (sample_rate > 0) ? sample_rate : 44100;

    // Always create a fresh Csound instance
    csound = std::make_unique<Csound>();

    csound->SetHostData(this);
    set_host_io();
    csound->SetMessageCallback(csound_message_callback);

    set_csound_midi_callbacks();

    // Make sure no stale FIFO state survives Csound re-initialization.
    audio_buffers.clear();
    midi_buffers.clear();

    // IMPORTANT: Csound's SetOption may store the pointer and read it later
    // during CompileCSD. We must ensure the strings stay alive until then.
    // Passing .c_str() of a temporary std::format result is a use-after-free!
    std::string opt_r = std::format("-r{}", safe_sample_rate);

    csound->SetOption("-d");
    csound->SetOption("-m0");
    csound->SetOption("-+rtmidi=NULL");
    csound->SetOption("-M0");
    csound->SetOption(opt_r.c_str());
    csound->SetOption("-Q0");

    if (io_layout.get_total_in_size() > 0) {
        csound->SetOption("-i adc");
    }

    if (io_layout.get_out_size() > 0) {
        csound->SetOption("-o dac");
    } else {
        csound->SetOption("-n");
    }

    is_compiling = true;
    compilation_log_buffer.clear();

    int compile_result = csound->CompileCSD(csd_file_content.c_str(), 1);
    int start_result = 0;

    if (compile_result == 0) {
        // Do not re-assert host I/O here.
        //
        // Some Csound versions expect host I/O configuration to remain stable
        // around compilation/start. Re-asserting it after CompileCSD() can
        // disturb the audio I/O state and cause the plugin to go silent.
        start_result = csound->Start();
    }

    is_compiling = false;

    if (compile_result != 0 || start_result != 0) {
        // FAILURE: Copy the main-thread string into the RT-safe buffer
        const char* error_text =
            compilation_log_buffer.empty()
                ? "Csound compilation or start failed"
                : compilation_log_buffer.c_str();

        set_last_error(error_text);
        log(csd_plugin::LogLevel::Error, error_text);

        ready_to_play.store(false, std::memory_order_release);
        std::string().swap(compilation_log_buffer);
        return false;
    }

    // SUCCESS: Clear any previous errors
    clear_last_error();
    std::string().swap(compilation_log_buffer);
    return true;
}

void Processor::prepare_to_play(int sample_rate, int max_block_size)
{
    const int safe_sample_rate = (sample_rate > 0) ? sample_rate : 44100;

    // Keep the processor inactive until the whole preparation is complete.
    ready_to_play.store(false, std::memory_order_release);
    clear_last_error();

    if (!prepare_csound_to_play(safe_sample_rate)) {
        prepared_sample_rate = 0;
        stop_and_reset_csound();
        log(LogLevel::Error, "prepare_to_play: prepare_csound_to_play failed");
        return;
    }

    if (!validate_io_layout()) {
        prepared_sample_rate = 0;
        stop_and_reset_csound();
        log(LogLevel::Error, "prepare_to_play: validate_io_layout failed");
        return;
    }

    if (csound_settings.sample_rate != safe_sample_rate) {
        log(
            LogLevel::Warning,
            "Csound sample rate differs from host sample rate after initialization"
        );
    }

    current_sample = 0;
    current_cycle_start_sample = 0;
    current_cycle_end_sample = 0;
    midi_buffers.clear();

    prepare_audio_buffers(max_block_size);

    // Only now is the processor fully usable.
    ready_to_play.store(true, std::memory_order_release);
    log(LogLevel::Info, "prepare_to_play: success, ready_to_play = true");
}

bool Processor::prepare_csound_to_play(int sample_rate) {
    //
    // Re-initialize Csound when:
    // - Csound instance is missing,
    // - host sample rate changed since last successful preparation.
    //
    // We intentionally do not parse or rewrite the CSD file.
    // Csound command-line flags are responsible for the sample-rate override.
    //
    const bool needs_csound_reinit =
        (csound == nullptr) ||
        (prepared_sample_rate != sample_rate);

    // NOTE: We intentionally DO NOT spin on is_processing() here.
    // Many DAWs suspend the audio thread during sample-rate switches.
    // If the audio thread was suspended inside processBlock(), spinning
    // here would deadlock the main thread and cause prepareToPlay to fail.
    // We rely on the host's guarantee that processBlock is not running
    // concurrently with prepareToPlay.

    if (needs_csound_reinit) {
        // The k-rate callback belongs to the previous Csound processing setup.
        // It will be set again by the JUCE layer after successful preparation.
        krate_callback = nullptr;

        stop_and_reset_csound();

        if (!setup_csound(sample_rate)) {
            prepared_sample_rate = 0;
            return false;
        }

        csound_settings.prepare(csound.get());
        prepared_sample_rate = sample_rate;
    } else {
        if (!csound) {
            prepared_sample_rate = 0;
            return false;
        }

        // Csound is already prepared at this sample rate.
        //
        // Do not re-query Csound settings here. Some hosts/JUCE wrappers can call
        // prepareToPlay() multiple times, and re-querying Csound while it is already
        // prepared can create inconsistent state. Keep the existing settings and only
        // normalize values that are authoritative for the plugin.
        prepared_sample_rate = sample_rate;
    }

    const int layout_out = io_layout.get_out_size();
    const int layout_in = io_layout.get_total_in_size();

    // Some Csound versions/host states can return unreliable audio channel counts,
    // especially while the host is reopening audio IO after a sample-rate switch.
    // For plugin readiness, the declared IOLayout is authoritative.
    if (csound_settings.ksmps <= 0) {
        log(
            LogLevel::Warning,
            std::format(
                "Csound reported invalid ksmps={}. Falling back to ksmps=1.",
                csound_settings.ksmps
            ).c_str()
        );
        csound_settings.ksmps = 1;
    }

    if (csound_settings.sample_rate <= 0) {
        csound_settings.sample_rate = sample_rate;
    }

    csound_settings.out_size = layout_out;
    csound_settings.in_size = layout_in;

    return true;
}


void Processor::prepare_audio_buffers(int max_block_size)
{
    const int in_size = io_layout.get_total_in_size();
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    const int desired_in_capacity =
        get_desired_audio_buffer_size(in_size, max_block_size, ksmps);

    const int desired_out_capacity =
        get_desired_audio_buffer_size(out_size, max_block_size, ksmps);

    audio_buffers.reset(desired_in_capacity, desired_out_capacity);

    current_max_block_size = std::max(max_block_size, MIN_FIFO_BLOCK_FRAMES);

    resync_audio_buffers();
}

void Processor::process_block(int block_size)
{
    if (block_size <= 0)
        return;

    // Protect the whole internal processing step.
    // The JUCE layer should also use begin_processing_scope()/end_processing_scope()
    // around host FIFO transfers so prepare/release cannot reset state concurrently.
    ScopedProcessingScope scope(*this);

    if (!ready_to_play.load(std::memory_order_acquire))
        return;

    const int in_size = io_layout.get_total_in_size();
    const int out_size = io_layout.get_out_size();

    bool capacity_ok = true;

    if (in_size > 0) {
        const int in_capacity = audio_buffers.in().get_capacity();

        if (in_capacity <= 0 || block_size > in_capacity / in_size) {
            capacity_ok = false;
        }
    }

    if (out_size > 0) {
        const int out_capacity = audio_buffers.out().get_capacity();

        if (out_capacity <= 0 || block_size > out_capacity / out_size) {
            capacity_ok = false;
        }
    }

    if (!capacity_ok) {
//        set_last_error("Host block size exceeds internal FIFO capacity; resyncing");
//        log(LogLevel::Error, "Host block size exceeds internal FIFO capacity; resyncing");

        resync_audio_buffers();
        ensure_output_for_block(block_size);

        return;
    }

    csound_process(block_size);
    ensure_output_for_block(block_size);
}

void Processor::release_resources() {
    //
    // Soft release / suspend.
    //
    // Important:
    // Some hosts call releaseResources() during sample-rate switches,
    // sometimes after prepareToPlay() has already prepared the plugin.
    // If we destroy Csound here, the plugin can remain silent until the user
    // manually deactivates/activates it.
    //
    // Therefore we only clear real-time FIFO state and timing counters here.
    // Full destruction is done in shutdown(), called from the destructor.
    //
    // NOTE: No spinning on is_processing(). The host guarantees thread safety.
    clear_buffers();

    current_sample = 0;
    current_cycle_start_sample = 0;
    current_cycle_end_sample = 0;
}

void Processor::shutdown() {
    ready_to_play.store(false, std::memory_order_release);
    prepared_sample_rate = 0;

    krate_callback = nullptr;
    log_callback = nullptr;

    stop_and_reset_csound();

    clear_buffers();

    current_sample = 0;
    current_cycle_start_sample = 0;
    current_cycle_end_sample = 0;
    current_max_block_size = 0;
}


static int ceil_div(int a, int b)
{
    return (a + b - 1) / b;
}

int Processor::get_csound_cycle_size(int block_size)
{
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    if (block_size <= 0 || out_size <= 0 || ksmps <= 0)
        return 0;

    const int available_out_frames =
        audio_buffers.out().get_size() / out_size;

    const int missing_frames = block_size - available_out_frames;

    if (missing_frames <= 0)
        return 0;

    return ceil_div(missing_frames, ksmps);
}

void Processor::csound_process(int block_size)
{
    const int in_size = io_layout.get_total_in_size();
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    if (block_size <= 0 || out_size <= 0 || ksmps <= 0)
        return;

    const int in_cycle_samples = ksmps * in_size;
    const int out_cycle_samples = ksmps * out_size;

    csound_cycle_size = get_csound_cycle_size(block_size);

    // If the total required output cannot fit in the current free space,
    // the FIFO watermark has been violated. Recover before processing.
    if (csound_cycle_size > 0) {
        const int64_t required_out_samples =
            static_cast<int64_t>(csound_cycle_size) * out_cycle_samples;

        if (required_out_samples > audio_buffers.out().get_free_space()) {
            set_last_error("Csound output FIFO watermark violated; resyncing");
            log(LogLevel::Error, "Csound output FIFO watermark violated; resyncing");

            resync_audio_buffers();

            // Recompute after resync.
            csound_cycle_size = get_csound_cycle_size(block_size);
        }
    }

    const int64_t block_start_sample = current_sample;
    const int64_t block_end_sample = block_start_sample + block_size;

    // The output FIFO may already contain samples for the current host block
    // because of latency prefill or leftover frames from previous blocks.
    const int64_t initial_out_frames =
        static_cast<int64_t>(audio_buffers.out().get_size()) / static_cast<int64_t>(out_size);

    int64_t next_cycle_output_start_sample = block_start_sample + initial_out_frames;

    current_cycle_start_sample = block_start_sample;
    current_cycle_end_sample = block_end_sample;

    for (int cycle_index = 0; cycle_index < csound_cycle_size; ++cycle_index) {
        // MIDI timing for this Csound cycle.
        //
        // Csound consumes input MIDI events up to the end of the audio frames
        // generated by this cycle. Output MIDI events are stamped with the start
        // of this cycle.
        current_cycle_start_sample = next_cycle_output_start_sample;
        current_cycle_end_sample = current_cycle_start_sample + ksmps;
        next_cycle_output_start_sample += ksmps;

        // This should not happen after the total watermark check above,
        // but keep it as a per-cycle safety guard.
        if (audio_buffers.out().get_free_space() < out_cycle_samples) {
            set_last_error("Csound output FIFO overflow risk; resyncing");
            log(LogLevel::Error, "Csound output FIFO overflow risk; resyncing");

            resync_audio_buffers();
            break;
        }

        if (in_size > 0) {
            MYFLT* spin = csound->GetSpin();

            if (spin == nullptr) {
                set_last_error("Csound input buffer is null; resyncing");
                log(LogLevel::Error, "Csound input buffer is null; resyncing");

                resync_audio_buffers();
                break;
            }

            // IMPORTANT:
            // Use partial read + zero-fill.
            // Never use all-or-nothing read_block() for Csound input,
            // because a failed atomic read would leave partial input stranded
            // in the FIFO and desynchronize input/output timing.
            const int was_read =
                audio_buffers.in().read_block_partial(spin, in_cycle_samples);

            std::fill(
                spin + was_read,
                spin + in_cycle_samples,
                MYFLT{0}
            );
        }

        if (krate_callback) {
            krate_callback();
        }

        int perform_result = csound->PerformKsmps();
        if (perform_result != 0) {
            // If the score ends or Csound stops, rewind the score to keep it running.
            // This prevents the plugin from going silent if the user forgets an infinite f-statement.
            csound->RewindScore();
            log(LogLevel::Warning, "Csound score ended or stopped; rewinding to keep alive.");
        }

        const MYFLT* spout = csound->GetSpout();

        if (spout == nullptr) {
            set_last_error("Csound output buffer is null; resyncing");
            log(LogLevel::Error, "Csound output buffer is null; resyncing");

            resync_audio_buffers();
            break;
        }

        const bool write_success =
            audio_buffers.out().write_block(spout, out_cycle_samples);

        if (!write_success) {
            set_last_error("Csound output FIFO write failed; resyncing");
            log(LogLevel::Error, "Csound output FIFO write failed; resyncing");

            resync_audio_buffers();
            break;
        }
    }

    // Advance the global sample counter by the host block size.
    // This keeps MIDI timestamps perfectly synced with the host timeline.
    current_sample += block_size;
    current_cycle_start_sample = current_sample;
    current_cycle_end_sample = current_sample;
}

void Processor::clear_buffers() {
    audio_buffers.clear();
    midi_buffers.clear();
}

int64_t Processor::get_current_sample() const {
    return current_sample;
}

void Processor::csound_message_callback(CSOUND* csound, int attr, const char* format, va_list val)
{
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, val);

    int type = attr & 0x7;
    csd_plugin::LogLevel level = csd_plugin::LogLevel::Info;

    if (type == 1) {
        level = csd_plugin::LogLevel::Error;
    } else if (type == 2) {
        level = csd_plugin::LogLevel::Warning;
    }

    auto* processor = static_cast<csd_plugin::Processor*>(csoundGetHostData(csound));

    if (!processor) {
        return;
    }

    if (processor->is_compiling) {
        // MAIN THREAD ONLY: Safe to use std::string concatenation
        std::string msg(buffer);

        if (!msg.empty() && msg != "\n") {
            if (processor->compilation_log_buffer.size() < MAX_COMPILATION_LOG_CHARS) {
                processor->compilation_log_buffer += msg;

                if (msg.back() != '\n') {
                    processor->compilation_log_buffer += '\n';
                }

                if (processor->compilation_log_buffer.size() > MAX_COMPILATION_LOG_CHARS) {
                    processor->compilation_log_buffer.resize(MAX_COMPILATION_LOG_CHARS);
                }
            }
        }
    } else {
        // AUDIO THREAD: 100% RT-safe logging
        processor->log(level, buffer);

        // If a runtime error occurs, capture it without allocation
        if (level == csd_plugin::LogLevel::Error) {
            processor->set_last_error(buffer);
        }
    }
}

bool Processor::validate_io_layout()
{
    if (!csound) {
        set_last_error("Csound instance is null");
        return false;
    }

    // Corrective validation:
    //
    // The declared IOLayout is authoritative for the plugin bus/channel configuration.
    // Do not fail preparation because of unreliable Csound channel queries.
    if (csound_settings.ksmps <= 0) {
        csound_settings.ksmps = 1;
    }

    if (csound_settings.sample_rate <= 0) {
        csound_settings.sample_rate =
            (prepared_sample_rate > 0)
                ? prepared_sample_rate
                : 44100;
    }

    csound_settings.out_size = io_layout.get_out_size();
    csound_settings.in_size = io_layout.get_total_in_size();

    return true;
}

void Processor::resync_audio_buffers()
{
    audio_buffers.clear();
    midi_buffers.clear();

    current_cycle_start_sample = current_sample;
    current_cycle_end_sample = current_sample;

    const int in_size = io_layout.get_total_in_size();
    const int out_size = io_layout.get_out_size();
    const int ksmps = csound_settings.ksmps;

    // For FX plugins, prefill one Csound cycle of output silence.
    //
    // This creates the expected initial latency and prevents first-block
    // input underflow when block_size and ksmps are not multiples.
    if (in_size > 0 && out_size > 0 && ksmps > 0) {
        int prefill_size = ksmps * out_size;
        for (int i = 0; i < prefill_size; ++i) {
            audio_buffers.out().write(static_cast<MYFLT>(0.0));
        }
    }
}

void Processor::ensure_output_for_block(int block_size)
{
    const int out_size = io_layout.get_out_size();

    if (out_size <= 0 || block_size <= 0)
        return;

    const int needed_samples = block_size * out_size;
    int missing_samples = needed_samples - audio_buffers.out().get_size();

    if (missing_samples <= 0)
        return;

    // Emergency padding.
    // This should be rare. If it happens often, the FIFO watermark/capacity
    // model has been violated.
    constexpr int chunk_size = 256;
    MYFLT zeros[chunk_size] = {};

    while (missing_samples > 0) {
        const int n = std::min(missing_samples, chunk_size);

        if (!audio_buffers.out().write_block(zeros, n)) {
            set_last_error("Csound output FIFO underflow and padding failed");
            log(LogLevel::Error, "Csound output FIFO underflow and padding failed");
            return;
        }

        missing_samples -= n;
    }

    set_last_error("Csound output FIFO underflow; padded with silence");
    log(LogLevel::Warning, "Csound output FIFO underflow; padded with silence");
}

}
