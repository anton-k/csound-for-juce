
#include <atomic>
#include <csd_plugin/audio/AudioBuffer.h>
#include <csd_plugin/audio/FastFifo.h>

namespace csd_plugin {

bool AudioBuffer::write(double sample) {
  return queue.push(sample);
}

bool AudioBuffer::read(double& sample) {
  return queue.read(sample);
}

bool AudioBuffer::write_block(const double* data, int num_items) {
  return queue.write_block(data, num_items);
}

bool AudioBuffer::read_block(double* dest, int num_items) {
  return queue.read_block(dest, num_items);
}

int AudioBuffer::get_size() {
  return queue.get_size();
}

int AudioBuffer::get_capacity() {
  return current_capacity.load(std::memory_order_relaxed);
}

void AudioBuffer::reset(int capacity) {
  if (current_capacity.load() != capacity) {
    queue.reset(capacity);
  } else {
    clear();
  }
}

void AudioBuffer::clear() {
  queue.clear();
}

}
