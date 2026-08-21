#pragma once

#include <readerwriterqueue.h>
using namespace moodycamel;

namespace juce_csd {

class Buffers {
  public:
    Buffers();
    Buffers(int size);
    Buffers(int input_size, int out_size);

    void write_input(float);
    void read_input(float&);
    void write_output(float);
    void read_output(float&);
    int output_size();
    void clear();

  private:
    ReaderWriterQueue<float> input_buffer{};
    ReaderWriterQueue<float> output_buffer{};
    std::atomic<int> out_size{0};

};

}
