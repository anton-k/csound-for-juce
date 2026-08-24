#pragma once

#include <atomic>
#include <memory>
#include <readerwriterqueue.h>
using namespace moodycamel;

namespace csd_plugin {

class AudioBuffer {
  public:
    AudioBuffer(int capacity):
      queue(std::make_unique<ReaderWriterQueue<float>>(capacity)),
      current_capacity(capacity),
      size(0)
    {}

    bool write(float);
    bool read(float&);
    void clear();
    int get_size();
    int get_capacity();
    void reset(int capacity);

  private:
    std::unique_ptr<ReaderWriterQueue<float>> queue{new ReaderWriterQueue<float>(0)};
    std::atomic<int> current_capacity{0};
    std::atomic<int> size{0};
};

class AudioBuffers {
  public:
    AudioBuffers();
    AudioBuffers(int size): AudioBuffers(size, size) {};
    AudioBuffers(int in_size, int out_size): input_buffer(in_size), output_buffer(out_size) {};

    void reset(int in_size, int out_size) {
      input_buffer.reset(in_size);
      output_buffer.reset(out_size);
    };

    void clear() {
      input_buffer.clear();
      output_buffer.clear();
    };

    AudioBuffer& in() { return input_buffer; };
    AudioBuffer& out() { return output_buffer; };

  private:
    AudioBuffer input_buffer;
    AudioBuffer output_buffer;
};

}
