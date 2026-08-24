
#include <atomic>
#include <memory>
#include <readerwriterqueue.h>
#include <juce_csd/audio/AudioBuffer.h>

using namespace moodycamel;
namespace csd_plugin {

bool AudioBuffer::write(float sample) {
  bool is_ok = queue->try_enqueue(sample);
  if (is_ok) {
    size++;
  }
  return is_ok;
}

bool AudioBuffer::read(float& sample) {
  bool is_ok = queue->try_dequeue(sample);
  if (is_ok) {
      size.fetch_sub(1, std::memory_order_relaxed);
  }
  return is_ok;
}

int AudioBuffer::get_size() {
  return size.load(std::memory_order_relaxed);
}

int AudioBuffer::get_capacity() {
  return current_capacity.load(std::memory_order_relaxed);
}

void AudioBuffer::reset(int capacity) {
  if (current_capacity.load() != capacity) {
    queue.reset();
    queue = std::make_unique<ReaderWriterQueue<float>>(capacity);
    current_capacity.store(capacity);
    size.store(0);
  } else {
    clear();
  }
}

void AudioBuffer::clear() {
  float sample;
  while (queue->try_enqueue(sample));
}

}
