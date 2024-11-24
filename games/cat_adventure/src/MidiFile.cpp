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

    const auto trackDivision = byteswap16(fr.GetUInt16());
    ramsyscall_printf("trackDivision : %d\n", trackDivision);

    for (int i = 0; i < trackNum; ++i) {
        const auto trackHeader = byteswap32(fr.GetUInt32());
        // TODO assert(trackHeader == 0x4D54726B)
        const auto chunkSize = byteswap32(fr.GetUInt32()); // should be 6
        (void)chunkSize;
        ramsyscall_printf("trackNum: %d, chunkSize: %d\n", i, chunkSize);

        auto cursor = 0;

        MidiEvent event;

        if (i == 5 || i == 1 || i == 4 || i == 6 || i == 3) {
            for (int j = 0; j < 100500; ++j) {
                // TODO: put into a function?
                auto b = fr.GetUInt8();
                ++cursor;
                event.delta = b;
                if (b & 0x80) {
                    event.delta = (b & 0x7F);
                    do {
                        b = fr.GetUInt8();
                        ++cursor;
                        event.delta = (event.delta << 7) + (b & 0x7F);
                    } while (b & 0x80);
                }

                // type
                event.type = MidiEvent::Type{(fr.GetUInt8() & 0xF0) >> 4};
                ++cursor;

                if (event.type != MidiEvent::Type::NoteOff &&
                    event.type != MidiEvent::Type::NoteOn &&
                    event.type != MidiEvent::Type::Controller &&
                    event.type != MidiEvent::Type::ProgramChange &&
                    event.type != MidiEvent::Type::PitchBend) {
                    ramsyscall_printf("UNKNOWN: 0x%01X\n", (int)event.type);
                }

                // param1
                event.param1 = fr.GetUInt8();
                ++cursor;

                if (event.type == MidiEvent::Type::NoteOff ||
                    event.type == MidiEvent::Type::NoteOn ||
                    event.type == MidiEvent::Type::Controller ||
                    event.type == MidiEvent::Type::PitchBend ||
                    event.type == MidiEvent::Type::Unknown) {
                    event.param2 = fr.GetUInt8();
                    ++cursor;
                }

                if (i == 3000 && event.type == MidiEvent::Type::NoteOn) {
                    ramsyscall_printf(
                        "%d: delta: %d, type : 0x%01X, param1: %d, param2: %d\n",
                        j,
                        event.delta,
                        event.type,
                        event.param1,
                        event.param2);
                }

                events[i].push_back(event);
                if (cursor == chunkSize) {
                    ramsyscall_printf("~~~ %d, %d messages\n", i, events[i].size());
                    goto endTrack;
                }
            }
        }

        fr.SkipBytes(chunkSize - cursor);

    endTrack:
    }
}
