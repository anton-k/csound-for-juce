
#include <readerwriterqueue.h>
#include "Buffer.hpp"

using namespace moodycamel;
namespace juce_csd {

Buffers::Buffers(int size): Buffers(size, size) {}

Buffers::Buffers(int input_size, int output_size):
  input_buffer{ReaderWriterQueue<float>(static_cast<size_t>(input_size))},
  output_buffer{ReaderWriterQueue<float>(static_cast<size_t>(output_size))}
{}

Buffers::Buffers(): Buffers(24000) {}

void Buffers::write_input(float sample) {
  input_buffer.try_enqueue(sample);
}

void Buffers::read_input(float &sample) {
  if (!input_buffer.try_dequeue(sample)) {
      sample = 0.0f;
  }
}

void Buffers::write_output(float sample) {
  output_buffer.try_enqueue(sample);
  out_size++;
}

void Buffers::read_output(float &sample) {
  if (output_buffer.try_dequeue(sample)) {
      out_size.fetch_sub(1, std::memory_order_relaxed);
  } else {
      sample = 0.0f; // Output silence if queue is empty
  }

}

int Buffers::output_size() {
  return out_size.load(std::memory_order_relaxed);
}

void Buffers::clear() {
    float temp;
    // Drain both queues safely
    while (input_buffer.try_dequeue(temp));
    while (output_buffer.try_dequeue(temp));

    // Reset the counter
    out_size.store(0, std::memory_order_relaxed);
}


}
