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
    void initReverb();
    void transferVAGToSpu(const Sound& sound, int voicechannel);
    void playAudio(int voicechannel);
};
