#pragma once

#include <EASTL/string_view.h>
#include <EASTL/vector.h>

struct MidiEvent {
    enum class Type {
        NoteOff = 0x8,
        NoteOn = 0x9,
        Controller = 0xB,
        ProgramChange = 0xC,
        PitchBend = 0xE,
        Unknown,
    };

    Type type{Type::Unknown};
    uint32_t delta;
    uint8_t param1{};
    uint8_t param2{};
};

struct MidiFile {
    void load(eastl::string_view filename, const eastl::vector<uint8_t>& data);

    eastl::vector<eastl::vector<MidiEvent>> events;
};
