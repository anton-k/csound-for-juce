#pragma once

#include <cstdint>
#include <cstring>
#include <csd_plugin/audio/FastFifo.h>

namespace csd_plugin {

inline constexpr int MIDI_DATA_SIZE = 4;

/// Raw midi event
struct RawMidiEvent {
    int64_t samplePosition;  // Timestamp relative to the start of the current audio block
    uint8_t size;            // Number of valid bytes (1 to 3 for standard MIDI)
    uint8_t data[MIDI_DATA_SIZE];         // Raw MIDI bytes (padded to 4 bytes for perfect 32-bit memory alignment)

    // Default constructor
    RawMidiEvent() : samplePosition(0), size(0), data{0, 0, 0, 0} {}

    // Constructor from raw bytes
    RawMidiEvent(int32_t pos, const uint8_t* rawData, uint8_t rawDataSize)
        : samplePosition(pos), size(rawDataSize > MIDI_DATA_SIZE ? MIDI_DATA_SIZE : rawDataSize)
    {
        // Fast, zero-allocation copy.
        // We cap at 4 bytes to silently ignore SysEx and guarantee RT-safety.
        if (rawData != nullptr) {
            std::memcpy(data, rawData, size);
        }
    }
};

/// Contains queue of midi events (excluding SysEx)
class MidiBuffer {
  public:
    MidiBuffer(size_t size): midi_buffer(std::max(size, (size_t)16)) {};

    /// Clears midi buffer
    void clear();

    /// Push midi event to the buffer
    bool push(const RawMidiEvent& msg);

    /// Peek the top event from the buffer
    bool peek(RawMidiEvent& outEvent);

    /// Remove top event from the buffer
    void pop();

    /// Reads the event from the buffer. Peeks and pops it.
    bool read(RawMidiEvent& outEvent);

  private:
    FastFifo<RawMidiEvent> midi_buffer;
};

/// Input and output midi buffers
class MidiBuffers {
  public:
    MidiBuffers(size_t in_size, size_t out_size): in_buffer{in_size}, out_buffer{out_size} {};

    /// Get reference to the input midi buffer
    MidiBuffer& in() { return in_buffer; };

    /// Get reference to the output midi buffer
    MidiBuffer& out() { return out_buffer; };

    /// Clear both buffers
    void clear() {
      in_buffer.clear();
      out_buffer.clear();
    };

  private:
    MidiBuffer in_buffer;
    MidiBuffer out_buffer;
};


}
