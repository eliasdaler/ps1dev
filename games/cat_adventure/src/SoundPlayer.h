#pragma once

#include <cstdint>

#include <EASTL/span.h>
#include <EASTL/string_view.h>
#include <EASTL/vector.h>

struct ToneAttribute;

enum class SpuReverbPreset : std::uint16_t {
    Room,
    StudioSmall,
    StudioMedium,
    StudioLarge,
    Hall,
    HalfEcho,
    SpaceEcho,
    ChaosEcho,
    Delay,
    Off,
};

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
    void setSpuState(uint16_t spuState);

    void setKeyOnOff(uint32_t keyOn, uint32_t keyOff);

    void setReverbPreset(SpuReverbPreset preset);
    void setReverbSettings(eastl::span<const std::uint16_t> settings, std::uint16_t reverbSize);

    uint16_t spuState = 0;

    static bool reverbEnabled;
};
