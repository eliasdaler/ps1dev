#include "MidiFile.h"

#include "FileReader.h"

#include <common/syscalls/syscalls.h>

namespace
{
uint32_t byteswap32(uint32_t x)
{
    uint32_t y = (x >> 24) & 0xff;
    y |= ((x >> 16) & 0xff) << 8;
    y |= ((x >> 8) & 0xff) << 16;
    y |= (x & 0xff) << 24;
    return y;
}

uint16_t byteswap16(uint16_t val)
{
    return (val << 8) | ((val >> 8) & 0xFF);
}

}

void MidiFile::load(eastl::string_view filename, const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto header = byteswap32(fr.GetUInt32());
    bool isMIDI = (header == 0x4D546864);
    if (!isMIDI) {
        ramsyscall_printf("%s is not a MIDI file\n", filename);
    }

    const auto chunkSize = byteswap32(fr.GetUInt32()); // should be 6
    (void)chunkSize;

    const auto formatType = byteswap16(fr.GetUInt16()); // should be 1 (for now)

    const auto trackNum = byteswap16(fr.GetUInt16());
    ramsyscall_printf("numTracks : %d\n", trackNum);

    events.resize(trackNum);

    ticksPerQuarter = byteswap16(fr.GetUInt16());
    ramsyscall_printf("ticksPerQuarter: %d\n", ticksPerQuarter);

    for (int i = 0; i < trackNum; ++i) {
        const auto trackHeader = byteswap32(fr.GetUInt32());
        // TODO assert(trackHeader == 0x4D54726B)
        const auto chunkSize = byteswap32(fr.GetUInt32()); // should be 6
        (void)chunkSize;
        ramsyscall_printf("trackNum: %d, chunkSize: %d\n", i, chunkSize);

        MidiEvent event;

        auto readVar = [](util::FileReader& fr) {
            auto b = fr.GetUInt8();
            uint32_t var = b;
            if (b & 0x80) {
                var = (b & 0x7F);
                do {
                    b = fr.GetUInt8();
                    var = (var << 7) + (b & 0x7F);
                } while (b & 0x80);
            }
            return var;
        };

        auto readVarLen = [](util::FileReader& fr, uint8_t len) {
            auto var = 0;
            for (int i = 0; i < len; ++i) {
                var = (var << 8) + fr.GetUInt8();
            }
            return var;
        };

        while (true) {
            event.delta = readVar(fr);

            // type
            const auto eventByte = fr.GetUInt8();
            event.type = MidiEvent::Type{(eventByte & 0xF0) >> 4};

            if (event.type != MidiEvent::Type::NoteOff && event.type != MidiEvent::Type::NoteOn &&
                event.type != MidiEvent::Type::Controller &&
                event.type != MidiEvent::Type::ProgramChange &&
                event.type != MidiEvent::Type::MetaEvent &&
                event.type != MidiEvent::Type::PitchBend) {
                ramsyscall_printf("UNKNOWN: 0x%01X, 0x%01X\n", (int)event.type, (int)eventByte);
                return;
            }

            if (event.type == MidiEvent::Type::MetaEvent) {
                event.metaEventType = (MidiEvent::MetaEventType)fr.GetUInt8();
                // ramsyscall_printf("META EVENT: 0x%01X\n", event.metaEventType);
                if (event.metaEventType == MidiEvent::MetaEventType::SetTempo ||
                    event.metaEventType == MidiEvent::MetaEventType::TimeSignature) {
                    const auto len = fr.GetUInt8();
                    event.metaValue = readVarLen(fr, len);
                    // ramsyscall_printf("var len: len=%d, %d\n", (int)len, (int)event.metaValue);
                } else if (event.metaEventType == MidiEvent::MetaEventType::EndOfTrack) {
                    const auto len = fr.GetUInt8(); //  should be 0
                    goto endTrack;
                }
                events[i].push_back(event);
                continue;
            }

            // param1
            event.param1 = fr.GetUInt8();

            if (event.type == MidiEvent::Type::NoteOff || event.type == MidiEvent::Type::NoteOn ||
                event.type == MidiEvent::Type::Controller ||
                event.type == MidiEvent::Type::PitchBend ||
                event.type == MidiEvent::Type::Unknown) {
                event.param2 = fr.GetUInt8();
            }

            if (i == 3000 && event.type == MidiEvent::Type::NoteOn) {
                ramsyscall_printf(
                    "%d: delta: %d, type : 0x%01X, param1: %d, param2: %d\n",
                    event.delta,
                    event.type,
                    event.param1,
                    event.param2);
            }

            events[i].push_back(event);
        }

    endTrack:
    }
}
