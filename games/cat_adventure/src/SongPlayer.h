#pragma once

#include <cstdint>

#include <EASTL/array.h>

namespace psyqo
{
class GPU;
}

struct MidiFile;
struct VabFile;
struct SoundPlayer;

struct SongPlayer {
    SongPlayer(psyqo::GPU& gpu, SoundPlayer& spu);

    void init(MidiFile& song, VabFile& vab);
    void findBPM(MidiFile& song);

    void updateMusic();

    int findChannel(std::uint8_t trackId, std::uint8_t noteId);
    void freeChannel(std::uint8_t trackId, std::uint8_t noteId);

    // data
    std::uint32_t voicesKeyOnMask{0};
    std::uint32_t voicesKeyOffMask{0};

    static constexpr std::size_t SPU_VOICE_COUNT{24};
    eastl::array<bool, SPU_VOICE_COUNT> usedVoices;
    struct VoiceUseInfo {
        std::uint8_t trackId; // midi track
        std::uint8_t noteId; // midi note which was keyed on
    };
    eastl::array<VoiceUseInfo, SPU_VOICE_COUNT> voiceUsers;

    std::uint32_t bpm{120};
    std::uint32_t microsecondsPerClick{0};

    static constexpr std::size_t MAX_MIDI_TRACKS{16};
    eastl::array<std::size_t, MAX_MIDI_TRACKS> lastEventIdx{};
    eastl::array<std::size_t, MAX_MIDI_TRACKS> eventNum{};
    eastl::array<std::uint8_t, MAX_MIDI_TRACKS> currentInst{};

    unsigned musicTimer;
    int musicTime{0};

    psyqo::GPU& gpu;
    SoundPlayer& spu;

    MidiFile* song{nullptr};
    VabFile* vab{nullptr};

    // TODO: move somewhere else?
    // This is where uploaded PCM samples start on SPU
    static constexpr uint32_t spuPCMStartAddr = 0x1010;
};
