#pragma once

#include <cstdint>
enum class MidiInputType : uint8_t {
    NOTE_OFF       = 0x80,
    NOTE_ON        = 0x90,
    CONTROL_CHANGE = 0xB0,
    PROGRAM_CHANGE = 0xC0,
    PITCH_BEND     = 0xE0,
    NONE           = 0x00
};

struct RawMidiEvent {
    MidiInputType type;
    uint8_t channel;
    uint8_t data1; // Nota ou Número do CC
    uint8_t data2; // Velocity ou Valor do CC
};

