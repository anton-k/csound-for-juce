#pragma once

#include <cstdint>
#include <cstring>
#include <readerwriterqueue.h>
using namespace moodycamel;

namespace csd_plugin {

struct RawMidiEvent {
    int32_t samplePosition;  // Timestamp relative to the start of the current audio block
    uint8_t size;            // Number of valid bytes (1 to 3 for standard MIDI)
    uint8_t data[4];         // Raw MIDI bytes (padded to 4 bytes for perfect 32-bit memory alignment)

    // Default constructor
    RawMidiEvent() : samplePosition(0), size(0), data{0, 0, 0, 0} {}

    // Constructor from raw bytes
    RawMidiEvent(int32_t pos, const uint8_t* rawData, uint8_t rawDataSize)
        : samplePosition(pos), size(rawDataSize > 4 ? 4 : rawDataSize)
    {
        // Fast, zero-allocation copy.
        // We cap at 4 bytes to silently ignore SysEx and guarantee RT-safety.
        std::memcpy(data, rawData, size);
    }

    void copy(RawMidiEvent* that) {
      samplePosition = that->samplePosition;
      size = that->size;
      std::memcpy(data, that->data, that->size);
    }
};

class MidiBuffer {
  public:
    MidiBuffer(int size): midi_buffer(size) {};
    void clear();
    void push(const RawMidiEvent& msg);
    bool peek(RawMidiEvent& outEvent);
    void pop();

  private:
    ReaderWriterQueue<RawMidiEvent> midi_buffer;
};

class MidiBuffers {
  public:
    MidiBuffers(int in_size, int out_size): in_buffer{in_size}, out_buffer{out_size} {};

    MidiBuffer& in() { return in_buffer; };
    MidiBuffer& out() { return out_buffer; };

    void clear() {
      in_buffer.clear();
      out_buffer.clear();
    };

  private:
    MidiBuffer in_buffer;
    MidiBuffer out_buffer;
};


}
