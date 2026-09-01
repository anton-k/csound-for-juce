#pragma once

#include <atomic>
#include <csd_plugin/audio/FastFifo.h>

namespace csd_plugin {

/// Audio buffer for real-time queue. Audio buffer is used to read and write
// samples from the DAW, and read/write the samples to Csound for audio processing.
class AudioBuffer {
  public:
    AudioBuffer(int capacity):
      queue(capacity),
      current_capacity(capacity)
    {}

    /// Writes sample to buffer
    bool write(float);

    /// Reads sample from buffer
    bool read(float&);

    /// Clears the buffer
    void clear();

    /// Returns the size of the buffer (how many samples are in the buffer to read)
    int get_size();

    /// Returns total size of the buffer. How many samples it can hold in total.
    int get_capacity();

    /// Resets the buffer to the new capacity
    void reset(int capacity);

  private:
    FastFifo<float> queue;
    std::atomic<int> current_capacity{0};
};

/// Class for audio buffers (input and output buffers).
class AudioBuffers {
  public:
    AudioBuffers();

    /// Creates audio buffers with the same size for input and output buffers
    AudioBuffers(int size): AudioBuffers(size, size) {};

    /// Provides different values for input and output size of the audio buffer
    AudioBuffers(int in_size, int out_size): input_buffer(in_size), output_buffer(out_size) {};

    /// resets the capacities for the buffers to the new values. All values are cleared from the buffers
    void reset(int in_size, int out_size) {
      input_buffer.reset(in_size);
      output_buffer.reset(out_size);
    };

    /// Clear both input and output buffers
    void clear() {
      input_buffer.clear();
      output_buffer.clear();
    };

    /// Get reference to the input buffer
    AudioBuffer& in() { return input_buffer; };

    /// Get reference to the output buffer
    AudioBuffer& out() { return output_buffer; };

  private:
    AudioBuffer input_buffer;
    AudioBuffer output_buffer;
};

}
