#include <csd_plugin/audio/MidiBuffer.h>

namespace csd_plugin {

void MidiBuffer::clear() {
    midi_buffer.clear();
}

bool MidiBuffer::push(const RawMidiEvent& msg) {
    return midi_buffer.push(msg);
}

bool MidiBuffer::peek(RawMidiEvent& outEvent) {
    RawMidiEvent* evt = midi_buffer.peek();
    if (evt == nullptr) {
        return false;
    }
    outEvent = *evt;
    return true;
}

void MidiBuffer::pop() {
    midi_buffer.pop();
}

bool MidiBuffer::read(RawMidiEvent& outEvent) {
    return midi_buffer.read(outEvent);
}
};
