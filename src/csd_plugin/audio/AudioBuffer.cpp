
#include <atomic>
#include <csd_plugin/audio/AudioBuffer.h>
#include <csd_plugin/audio/FastFifo.h>

namespace csd_plugin {

bool AudioBuffer::write(float sample) {
  return queue.push(sample);
}

bool AudioBuffer::read(float& sample) {
  return queue.read(sample);
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
