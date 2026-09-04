#pragma once

#include <atomic>
#include <csd_plugin/audio/FastFifo.h>

namespace csd_plugin {

/// Audio buffer for real-time queue. Audio buffer is used to read and write
// samples from the DAW, and read/write the samples to Csound for audio processing.
template <typename Sample>
class AudioBuffer {
public:
    AudioBuffer() = default;

    explicit AudioBuffer(int capacity) {
        reset(capacity);
    }

    bool write(Sample sample) {
        return queue.push(sample);
    }

    bool read(Sample& sample) {
        return queue.read(sample);
    }

    bool write_block(const Sample* data, int num_items) {
        return queue.write_block(data, num_items);
    }

    bool read_block(Sample* dest, int num_items) {
        return queue.read_block(dest, num_items);
    }

    int read_block_partial(Sample* dest, int num_items) {
        return queue.read_block_partial(dest, num_items);
    }

    int get_size() const {
        return queue.get_size();
    }

    int get_capacity() const {
        return queue.get_capacity();
    }

    int get_free_space() const {
        return queue.get_free_space();
    }

    void clear() {
        queue.clear();
    }

    void reset(int capacity) {
        queue.reset(capacity);
    }

private:
    FastFifo<Sample> queue;
};


/// Class for audio buffers (input and output buffers).
template <typename Sample>
class AudioBuffers {
public:
    AudioBuffers() = default;

    AudioBuffers(int in_size, int out_size)
        : input_buffer(in_size), output_buffer(out_size) {}

    void reset(int in_size, int out_size) {
        input_buffer.reset(in_size);
        output_buffer.reset(out_size);
    }

    void clear() {
        input_buffer.clear();
        output_buffer.clear();
    }

    AudioBuffer<Sample>& in() {
        return input_buffer;
    }

    AudioBuffer<Sample>& out() {
        return output_buffer;
    }

private:
    AudioBuffer<Sample> input_buffer;
    AudioBuffer<Sample> output_buffer;
};

}
