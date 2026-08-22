
#include <readerwriterqueue.h>
#include <juce_csd/audio/AudioBuffer.h>

using namespace moodycamel;
namespace juce_csd {

AudioBuffers::AudioBuffers(int size): AudioBuffers(size, size) {}

AudioBuffers::AudioBuffers(int input_size, int output_size):
  input_buffer{ReaderWriterQueue<float>(static_cast<size_t>(input_size))},
  output_buffer{ReaderWriterQueue<float>(static_cast<size_t>(output_size))}
{}

void AudioBuffers::write_input(float sample) {
  input_buffer.try_enqueue(sample);
}

void AudioBuffers::read_input(float &sample) {
  if (!input_buffer.try_dequeue(sample)) {
      sample = 0.0f;
  }
}

void AudioBuffers::write_output(float sample) {
  output_buffer.try_enqueue(sample);
  out_size++;
}

void AudioBuffers::read_output(float &sample) {
  if (output_buffer.try_dequeue(sample)) {
      out_size.fetch_sub(1, std::memory_order_relaxed);
  } else {
      sample = 0.0f; // Output silence if queue is empty
  }

}

int AudioBuffers::output_size() {
  return out_size.load(std::memory_order_relaxed);
}

void AudioBuffers::clear() {
    float temp;
    // Drain both queues safely
    while (input_buffer.try_dequeue(temp));
    while (output_buffer.try_dequeue(temp));

    // Reset the counter
    out_size.store(0, std::memory_order_relaxed);
}


}
