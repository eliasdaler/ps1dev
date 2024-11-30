#pragma once

#include <cstdint>

#include <EASTL/string_view.h>
#include <EASTL/vector.h>

struct ToneAttribute;

struct Sound {
    eastl::vector<std::uint8_t> bytes;
    std::uint32_t sampleFreq{44100};
    std::uint32_t dataSize{0};
    mutable std::uint32_t startAddr;
    bool isVag{true};

    void load(eastl::string_view filename, const eastl::vector<uint8_t>& data);
};

struct SoundPlayer {
    void init();

    void resetVoice(int voiceID);
    void playSound(int channel, const Sound& sound, uint16_t pitch);
    void playSound(
        int channel,
        uint32_t startAddr,
        uint8_t velocity,
        uint16_t pitch,
        const ToneAttribute& toneAttrib);
    void uploadSound(uint32_t SpuAddr, Sound& sound);
    void uploadSound(uint32_t SpuAddr, const uint8_t* data, uint32_t size);

    void setDMAWriteState();
    void setReverbEnabled();
    void setStopState();
    void setSpuState(int spuState);

    void setKeyOnOff(int keyOn, int keyOff);

    uint16_t spuState = 0;

    static bool reverbEnabled;
};
