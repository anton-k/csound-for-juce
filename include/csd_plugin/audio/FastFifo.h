/* Examples of usage
With MIDI:

2. Update MidiBuffer.h

Now, update your MIDI buffer to use the new FastFifo<RawMidiEvent>.


// include/csd_plugin/audio/MidiBuffer.h
#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include "FastFifo.h" // Include the new header

namespace csd_plugin {

// ... (RawMidiEvent struct remains exactly the same) ...

class MidiBuffer {
  public:
    MidiBuffer(size_t size) : midi_buffer(std::max(size, (size_t)16)) {};

    void clear() { midi_buffer.clear(); }
    bool push(const RawMidiEvent& msg) { midi_buffer.push(msg); return true; }
    bool peek(RawMidiEvent& outEvent);
    void pop() { midi_buffer.pop(); }

    bool read(RawMidiEvent& outEvent) {
      return midi_buffer.read(outEvent);
    };

  private:
    FastFifo<RawMidiEvent> midi_buffer; // Replaced ReaderWriterQueue
};

// ... (MidiBuffers class remains exactly the same) ...


3. Update MidiBuffer.cpp

The implementation becomes incredibly simple and requires no external dependencies.


// src/csd_plugin/audio/MidiBuffer.cpp
#include <csd_plugin/audio/MidiBuffer.h>

namespace csd_plugin {

bool MidiBuffer::peek(RawMidiEvent& outEvent) {
    RawMidiEvent* evt = midi_buffer.peek();
    if (evt == nullptr) {
        return false;
    }
    outEvent = *evt;
    return true;
}

} // namespace csd_plugin

With Audio:

How to use it for Block Copying with Csound

Currently, your code pushes/pops 1 sample at a time in a loop. With FastAudioFifo, you can
de-interleave/interleave directly into Csound's spin and spout arrays using block copies.

Here is how you would update the inner loop in src/csd_plugin/audio/Processor.cpp
(csound_process method) to use block copying:


// Inside csound_process(int buffer_size)

// Allocate temporary block buffers once (e.g., as members of Processor)
// std::vector<float> temp_block; resized to ksmps in prepare_to_play

for (int cycle_index = 0; cycle_index < csound_cycle_size; ++cycle_index) {
    current_cycle_end_sample = current_sample + csound_settings.ksmps;

    if (in_size > 0) {
        double* spin = csound->GetSpin();

        // 1. Read a block of interleaved floats from our FIFO
        audio_buffers.in().read(temp_block.data(), csound_settings.ksmps * in_size);

        // 2. Copy to Csound's spin (converting float to double)
        for (int i = 0; i < csound_settings.ksmps * in_size; ++i) {
            spin[i] = static_cast<double>(temp_block[i]);
        }
    }

    csound->PerformKsmps();

    const double* spout = csound->GetSpout();
    int out_samples = csound_settings.ksmps * out_size;

    // 3. Copy from Csound's spout to temp block (converting double to float)
    for (int i = 0; i < out_samples; ++i) {
        temp_block[i] = static_cast<float>(spout[i]);
    }

    // 4. Write the block to our output FIFO
    audio_buffers.out().write(temp_block.data(), out_samples);

    current_sample = current_cycle_end_sample;
}

*/

#pragma once

#include <vector>
#include <cstring>
#include <type_traits>
#include <cstdint>

namespace csd_plugin {

template <typename T>
class FastFifo {
    static_assert(std::is_trivially_copyable_v<T>, "FastFifo requires trivially copyable types for fast memcpy");

public:
    FastFifo() = default;

    explicit FastFifo(int capacity) {
        reset(capacity);
    }

    void reset(int capacity) {
        size_ = 1;
        while (size_ < capacity) size_ <<= 1; // Power of 2
        mask_ = size_ - 1;

        buffer_.resize(size_);
        read_pos_ = 0;
        write_pos_ = 0;
    }

    // --- Single Item Operations ---

    bool push(const T& item) {
        if (get_size() >= size_) return false; // Prevent overwrite
        buffer_[write_pos_ & mask_] = item;
        ++write_pos_;
        return true;
    }

    bool read(T& dest) {
        if (get_size() == 0) return false;
        dest = buffer_[read_pos_ & mask_];
        ++read_pos_;
        return true;
    }

    T* peek() {
        if (get_size() == 0) return nullptr;
        return &buffer_[read_pos_ & mask_];
    }

    void pop() {
        if (get_size() > 0) ++read_pos_;
    }

    // --- Block Operations ---

    bool write_block(const T* data, int num_items) {
        if (get_size() + num_items > size_) return false; // Not enough space

        uint32_t write_idx = write_pos_ & mask_;
        uint32_t space_until_end = size_ - write_idx;

        if (space_until_end >= num_items) {
            std::memcpy(&buffer_[write_idx], data, num_items * sizeof(T));
        } else {
            std::memcpy(&buffer_[write_idx], data, space_until_end * sizeof(T));
            std::memcpy(&buffer_[0], data + space_until_end, (num_items - space_until_end) * sizeof(T));
        }
        write_pos_ += num_items;
        return true;
    }

    bool read_block(T* dest, int num_items) {
        if (get_size() < num_items) return false; // Not enough data

        uint32_t read_idx = read_pos_ & mask_;
        uint32_t space_until_end = size_ - read_idx;

        if (space_until_end >= num_items) {
            std::memcpy(dest, &buffer_[read_idx], num_items * sizeof(T));
        } else {
            std::memcpy(dest, &buffer_[read_idx], space_until_end * sizeof(T));
            std::memcpy(dest + space_until_end, &buffer_[0], (num_items - space_until_end) * sizeof(T));
        }
        read_pos_ += num_items;
        return true;
    }

    // --- State ---

    int get_size() const { return static_cast<int>(write_pos_ - read_pos_); }
    int get_capacity() const { return size_; }
    void clear() { read_pos_ = write_pos_ = 0; }

private:
    std::vector<T> buffer_;
    int size_ = 0;
    uint32_t mask_ = 0;
    uint32_t read_pos_ = 0;
    uint32_t write_pos_ = 0;
};

} // namespace csd_plugin

