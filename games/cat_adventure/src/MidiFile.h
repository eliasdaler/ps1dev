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
        MetaEvent = 0xF,
        Unknown,
    };

    enum class MetaEventType {
        SequenceNumber = 0x00,
        TextEvent = 0x01,
        CopyrightNotice = 0x02,
        TrackName = 0x03,
        InstrumentName = 0x04,
        TextLyric = 0x05,
        TextMarker = 0x06,
        CuePoint = 0x07,
        MidiChannelPrefix = 0x20,
        EndOfTrack = 0x2F,
        SetTempo = 0x51,
        SMPTEOffset = 0x54,
        TimeSignature = 0x58,
        sfmiKeySignature = 0x59,
        SequencerSpecific = 0x7F,
        Unknown,
    };

    Type type{Type::Unknown};
    uint32_t delta{};
    uint8_t param1{};
    uint8_t param2{};

    MetaEventType metaEventType{MetaEventType::Unknown};
    uint32_t metaValue{};
};

struct MidiFile {
    void load(eastl::string_view filename, const eastl::vector<uint8_t>& data);

    uint32_t ticksPerQuarter{480};
    eastl::vector<eastl::vector<MidiEvent>> tracks;
};
