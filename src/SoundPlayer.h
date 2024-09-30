#pragma once

#include <cstdint>

#include <libspu.h>
#include <libcd.h>

#include <types.h>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#define SOUND_MALLOC_MAX 10

struct Sound {
    eastl::vector<std::uint8_t> bytes;
    std::uint16_t sampleFreq{44100};

    void load(eastl::string_view filename);
};

struct SoundPlayer {
    SpuCommonAttr spucommonattr;
    SpuVoiceAttr spuvoiceattr;
    char spumallocrec[SPU_MALLOC_RECSIZ * (SOUND_MALLOC_MAX + 1)];
    u_long vagspuaddr;
    CdlLOC loc[100];
    int numtoc;

    void init();
    void initReverb();
    void transferVAGToSpu(const Sound& sound, int voicechannel);
    void playAudio(int voicechannel);
};
