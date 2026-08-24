
#include <atomic>
#include <memory>
#include <readerwriterqueue.h>
#include <csd_plugin/audio/AudioBuffer.h>

using namespace moodycamel;
namespace csd_plugin {

bool AudioBuffer::write(float sample) {
  return queue->try_enqueue(sample);
}

bool AudioBuffer::read(float& sample) {
  return queue->try_dequeue(sample);
}

int AudioBuffer::get_size() {
  return queue->size_approx();
}

int AudioBuffer::get_capacity() {
  return current_capacity.load(std::memory_order_relaxed);
}

void AudioBuffer::reset(int capacity) {
  if (current_capacity.load() != capacity) {
    queue.reset();
    queue = std::make_unique<ReaderWriterQueue<float>>(capacity);
    current_capacity.store(capacity);
  } else {
    clear();
  }
}

void AudioBuffer::clear() {
  float sample{0.f};
  while (queue->try_dequeue(sample));
}

}
