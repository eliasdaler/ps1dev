#include "SongPlayer.h"

#include "MidiFile.h"
#include "SoundPlayer.h"
#include "VabFile.h"

#include <common/hardware/hwregs.h>
#include <common/syscalls/syscalls.h>
#include <psyqo/fixed-point.hh>
#include <psyqo/gpu.hh>

namespace
{

constexpr auto MICROSECONDS_IN_MINUTE = 60 * 1000 * 1000;

// Taken from PCSX-Redux's modplayer
uint32_t calculateHBlanks(uint32_t bpm)
{
    // The original code only uses 39000 here but the reality is a bit more
    // complex than that, as not all clocks are exactly the same, depending
    // on the machine's region, and the video mode selected.

    uint32_t status = GPU_STATUS;
    int isPalConsole = *((const char*)0xbfc7ff52) == 'E';
    int isPal = (status & 0x00100000) != 0;
    uint32_t base;
    if (isPal && isPalConsole) { // PAL video on PAL console
        base = 39062; // 312.5 * 125 * 50.000 / 50 or 314 * 125 * 49.761 / 50
    } else if (isPal && !isPalConsole) { // PAL video on NTSC console
        base = 39422; // 312.5 * 125 * 50.460 / 50 or 314 * 125 * 50.219 / 50
    } else if (!isPal && isPalConsole) { // NTSC video on PAL console
        base = 38977; // 262.5 * 125 * 59.393 / 50 or 263 * 125 * 59.280 / 50
    } else { // NTSC video on NTSC console
        base = 39336; // 262.5 * 125 * 59.940 / 50 or 263 * 125 * 59.826 / 50
    }
    return base / bpm;
}

}

SongPlayer::SongPlayer(psyqo::GPU& gpu, SoundPlayer& spu) : gpu(gpu), spu(spu)
{}

void SongPlayer::init(MidiFile& song, VabFile& vab)
{
    for (int i = 0; i < lastEventIdx.size(); ++i) {
        lastEventIdx[i] = 0;
    }

    for (int i = 0; i < eventNum.size(); ++i) {
        eventNum[i] = 0;
    }

    for (int i = 0; i < eventNum.size(); ++i) {
        eventNum[i] = 0;
        voiceUsers[i] = {};
    }

    findBPM(song);

    this->song = &song;
    this->vab = &vab;

    if (song.tracks.size() > MAX_MIDI_TRACKS) {
        ramsyscall_printf(
            "Song has too many tracks: %d > %d\n", song.tracks.size(), MAX_MIDI_TRACKS);
    }

    // TODO: handle case where the init is called twice
    auto waitHBlanks = calculateHBlanks(bpm);
    musicTimer = gpu.armPeriodicTimer(
        waitHBlanks * psyqo::GPU::US_PER_HBLANK / 2, [this, &waitHBlanks](uint32_t) {
            updateMusic();
            musicTime += waitHBlanks * psyqo::GPU::US_PER_HBLANK / 2;

            waitHBlanks = calculateHBlanks(bpm);
            gpu.changeTimerPeriod(musicTimer, waitHBlanks * psyqo::GPU::US_PER_HBLANK / 2);
        });
}

void SongPlayer::findBPM(MidiFile& song)
{
    // FIXME: this assumes that the first even we encounter is in the first track
    // but actually we need to look for the first BPM change on the timeline
    // for all the tracks
    for (const auto& track : song.tracks) {
        if (track.empty()) {
            continue;
        }

        for (int i = 0; i < track.size(); ++i) {
            auto& event = track[i];
            if (event.metaEventType == MidiEvent::MetaEventType::SetTempo) {
                microsecondsPerClick = event.metaValue;
                bpm = MICROSECONDS_IN_MINUTE / microsecondsPerClick;
                return;
            }
        }
    }
}

void SongPlayer::updateMusic()
{
    // for debug
    const auto musicMuted = false;
    if (musicMuted) {
        return;
    }

    auto& spu = this->spu;
    const auto& tracks = song->tracks;
    const auto numTracks = tracks.size();
    const auto& vab = *this->vab;

    voicesKeyOnMask = 0;
    voicesKeyOffMask = 0;

    const auto tickTime = microsecondsPerClick / song->ticksPerQuarter;

    for (int trackIdx = 0; trackIdx < numTracks; ++trackIdx) {
        const auto& track = tracks[trackIdx];
        if (track.empty()) {
            continue;
        }

        /* if (trackIdx != 3) {
            continue;
        } */

        auto lastEventAbsTime = lastEventIdx[trackIdx];
        const auto trSize = track.size();
        const auto startEventIdx = eventNum[trackIdx];
        for (int i = startEventIdx; i < trSize; ++i) {
            const auto& event = track[i];
            if ((lastEventAbsTime + event.delta) * tickTime > musicTime) {
                // ^ FIXME: this is wrong if BPM changes - need to count in ticks!
                lastEventIdx[trackIdx] = lastEventAbsTime;
                eventNum[trackIdx] = i;
                break;
            }

            lastEventAbsTime = lastEventAbsTime + event.delta;
            if (event.type == MidiEvent::Type::MetaEvent) {
                if (event.metaEventType == MidiEvent::MetaEventType::SetTempo) {
                    microsecondsPerClick = event.metaValue;
                    bpm = MICROSECONDS_IN_MINUTE / microsecondsPerClick;
                }
            } else if (event.type == MidiEvent::Type::ProgramChange) {
                currentInst[trackIdx] = event.param1;
            } else if (event.type == MidiEvent::Type::NoteOn) {
                const auto note = event.param1;
                const auto voiceId = findChannel(trackIdx, note);
                if (voiceId == -1) {
                    // TODO: drop the oldest sample
                    continue;
                }

                const auto& inst = vab.instruments[currentInst[trackIdx]];

                auto toneIndex = -1;
                for (int k = 0; k < inst.numTones; ++k) {
                    const auto& tone = vab.toneAttributes[inst.tones[k]];
                    if (note >= tone.min && note <= tone.max) {
                        toneIndex = inst.tones[k];
                        break;
                    }
                }

                if (toneIndex == -1) {
                    continue;
                }

                const auto& tone = vab.toneAttributes[toneIndex];
                const auto baseNote = tone.center;

                auto addr = spuPCMStartAddr;
                for (int i = 0; i < tone.vag; ++i) {
                    addr += vab.vagSizes[i] << 3;
                }

                const auto pitchBase = note - baseNote;

                // TODO: LUT
                psyqo::FixedPoint<> pitch{1.0};
                psyqo::FixedPoint<> sqr{1.0594630943592};
                if (pitchBase >= 0) {
                    for (int i = 0; i < pitchBase; ++i) {
                        pitch *= sqr;
                    }
                } else {
                    for (int i = 0; i < -pitchBase; ++i) {
                        pitch /= sqr;
                    }
                }

                const auto velocity = event.param2;
                if (tone.mode != 4) {
                    reverbEnableMask &= ~(1 << voiceId);
                } else {
                    reverbEnableMask |= (1 << voiceId);
                }
                spu.playSound(voiceId, addr, velocity, pitch.value, tone);
                voicesKeyOnMask |= (1 << voiceId);
                // ramsyscall_printf("%d: on\n", update);
            } else if (event.type == MidiEvent::Type::NoteOff) {
                const auto note = event.param1;
                const auto voiceId = freeChannel(trackIdx, note);
                voicesKeyOffMask |= (1 << voiceId);
                reverbEnableMask &= ~(1 << voiceId);
                // ramsyscall_printf("%d: off\n", update);
            }
        }
    }

    spu.setKeyOnOff(voicesKeyOnMask, voicesKeyOffMask);
    spu.setReverbChannels(reverbEnableMask);
}

int SongPlayer::findChannel(std::uint8_t trackId, std::uint8_t noteId)
{
    for (std::uint8_t voiceId = 0; voiceId < usedVoices.size(); ++voiceId) {
        if (usedVoices[voiceId]) {
            continue;
        }

        voiceUsers[voiceId] = VoiceUseInfo{
            .trackId = trackId,
            .noteId = noteId,
        };
        usedVoices[voiceId] = true;
        return voiceId;
    }
    return -1;
}

int SongPlayer::freeChannel(std::uint8_t trackId, std::uint8_t noteId)
{
    for (std::uint8_t voiceId = 0; voiceId < usedVoices.size(); ++voiceId) {
        if (!usedVoices[voiceId]) {
            continue;
        }

        const auto& user = voiceUsers[voiceId];
        if (user.trackId == trackId && user.noteId == noteId) {
            usedVoices[voiceId] = false;
            return voiceId;
        }
    }
    return -1;
}
