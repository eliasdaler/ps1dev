#pragma once

#pragma once

#include <cstdint>

// #include <libspu.h>
// #include <libcd.h>

// #include <types.h>

#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#define SOUND_MALLOC_MAX 10

#define SPU_MALLOC_RECSIZ 8

struct Sound {
    eastl::vector<std::uint8_t> bytes;
    std::uint16_t sampleFreq{44100};
    std::uint16_t dataSize{0};
    mutable std::uint32_t startAddr;
    bool isVag{true};

    void load(eastl::string_view filename, const eastl::vector<uint8_t>& data);
};

struct SpuCommonAttr {};
struct SpuVoiceAttr {};
struct SpuReverbAttr {};

using CdlLOC = uint16_t;

struct SoundPlayer {
    SpuCommonAttr spucommonattr;
    SpuVoiceAttr spuVoiceAttr;
    SpuReverbAttr r_attr;
    int reverbMode;
    char spumallocrec[SPU_MALLOC_RECSIZ * (SOUND_MALLOC_MAX + 1)];
    uint32_t vagspuaddr;
    CdlLOC loc[100];
    int numtoc;

    void init();

    void resetVoice(int voiceID);
    void playSound(int channel, const Sound& sound, uint16_t pitch);
    void uploadSound(uint32_t SpuAddr, const Sound& sound);
    void uploadSound(uint32_t SpuAddr, const uint8_t* data, uint32_t size);

    void setDMAWriteState();
    void setStopState();
    void setSpuState(int spuState);

    int spuState = 0;
};
