
#include <csd_plugin/audio/MidiBuffer.h>

namespace csd_plugin {

void MidiBuffer::clear() {
      RawMidiEvent temp;
      while(midi_buffer.try_dequeue(temp));
}

bool MidiBuffer::push(const RawMidiEvent& msg) {
    return midi_buffer.try_enqueue(msg);
}


bool MidiBuffer::peek(RawMidiEvent& outEvent) {
  RawMidiEvent* evt = midi_buffer.peek();
  if (evt == nullptr) {
    return false;
  } else {
    outEvent = *evt;
    return true;
  }
}

void MidiBuffer::pop() {
  midi_buffer.pop();
}

};
