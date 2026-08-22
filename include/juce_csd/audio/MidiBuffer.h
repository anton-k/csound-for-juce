#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <readerwriterqueue.h>
using namespace moodycamel;

namespace juce_csd {

struct TimedMidiEvent {
  juce::MidiMessage message;
  int samplePosition;
};

class MidiBuffer {
  public:
    MidiBuffer() {};
    void clear();
    void push(const juce::MidiMessage& msg, int samplePos);
    bool peek(TimedMidiEvent& outEvent);
    void pop();

  private:
    ReaderWriterQueue<TimedMidiEvent> midi_buffer{1024};
};


}
