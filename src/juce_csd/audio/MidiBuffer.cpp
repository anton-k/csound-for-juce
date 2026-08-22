
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_csd/audio/MidiBuffer.h>

namespace juce_csd {

void MidiBuffer::clear() {
      TimedMidiEvent temp;
      while(midi_buffer.try_dequeue(temp));
}

void MidiBuffer::push(const juce::MidiMessage& msg, int samplePos) {
    midi_buffer.try_enqueue({msg, samplePos});
  }


bool MidiBuffer::peek(TimedMidiEvent& outEvent) {
  TimedMidiEvent* evt = midi_buffer.peek();
  if (evt == nullptr) {
    return false;
  } else {
    outEvent.message = evt->message;
    outEvent.samplePosition = evt->samplePosition;
    return true;
  }
}

void MidiBuffer::pop() {
  midi_buffer.pop();
}


};
