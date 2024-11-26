#pragma once

#include <EASTL/array.h>
#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#include <cstdint>

struct ProgramAttribute {
    std::uint8_t tones;
    std::uint8_t mvol;
    std::uint8_t prior;
    std::uint8_t mode;
    std::uint8_t mpan;
    std::uint8_t reserved0;
    std::uint16_t attr;
    std::uint32_t reserved1;
    std::uint32_t reserved2;
};

struct ToneAttribute {
    std::uint8_t prior; // priority
    std::uint8_t mode; // mode (0 - normal, 4 - reverb applied)
    std::uint8_t vol; // volume
    std::uint8_t pan; // pan
    std::uint8_t center; // center note
    std::uint8_t shift; // pitch correction (0~127,cent units)
    std::uint8_t min; // minimum note limit
    std::uint8_t max; // maximum note limit
    std::uint8_t vibW; // vibrato width
    std::uint8_t vibT; // 1 cycle time of vibrato (tick units)
    std::uint8_t porW; // portamento width
    std::uint8_t porT; // portamento holding time (tick units)
    std::uint8_t pbmin; // pitch bend min
    std::uint8_t pbmax; // pitch bend max
    std::uint8_t reserved1;
    std::uint8_t reserved2;
    std::uint8_t attack, decay; // ADSR1 (attack/decay)
    std::uint8_t release, sust; // ADSR2 (release/sustain)
    std::uint16_t prog; // parent program
    std::uint16_t vag; // waveform (VAG) used
    std::uint16_t reserved[4];
};

struct VabHeader {
    std::uint32_t fileID;
    std::uint32_t version;
    std::uint32_t vabId;
    std::uint32_t size;
    std::uint16_t reserved;
    std::uint16_t numPrograms;
    std::uint16_t numTones;
    std::uint16_t numVAGs;
    std::uint8_t masterVolume;
    std::uint8_t masterPan;
    std::uint8_t bankAttribute1;
    std::uint8_t bankAttribute2;
    std::uint32_t reserved2;
};

struct VabFile {
    void load(eastl::string_view filename, const eastl::vector<uint8_t>& data);

    VabHeader header;
    eastl::array<ProgramAttribute, 128> progAttributes;
    eastl::vector<ToneAttribute> toneAttributes;
    eastl::array<std::uint16_t, 256> vagSizes;
};
