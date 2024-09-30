#include "SoundPlayer.h"

#include "Utils.h"

#include <stdio.h>

#include <bit>

void SoundPlayer::init()
{
    SpuInit();
    SpuInitMalloc(SOUND_MALLOC_MAX, spumallocrec);

    // Set common attributes (master volume left/right, CD volume, etc.)
    spucommonattr.mask =
        (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR |
         SPU_COMMON_CDMIX);

    // Master volume (left) - should be between 0x0000 and 0x3FFF
    spucommonattr.mvol.left = 0x3FFF;
    spucommonattr.mvol.right = 0x3FFF; // Master volume (right)

    // CD volume (left)     -> should be between 0x0000 and 0x7FFF
    spucommonattr.cd.volume.left = 0x7FFF;
    spucommonattr.cd.volume.right = 0x7FFF; // CD volume (right)

    spucommonattr.cd.mix = SPU_ON; // Enable CD input on

    SpuSetCommonAttr(&spucommonattr);
    SpuSetTransferMode(SpuTransByDMA);

    initReverb();
}

void SoundPlayer::initReverb()
{
    // long mode = SPU_REV_MODE_ROOM | SPU_REV_MODE_STUDIO_A | SPU_REV_MODE_HALL |
    // SPU_REV_MODE_SPACE;

    // if (SpuReserveReverbWorkArea(1) == 1) {
    reverbMode = SPU_REV_MODE_OFF;

    r_attr.mask = (SPU_REV_MODE | SPU_REV_DEPTHL | SPU_REV_DEPTHR);
    r_attr.mode = (SPU_REV_MODE_CLEAR_WA | reverbMode);
    r_attr.depth.left = 0x3fff;
    r_attr.depth.right = 0x3fff;

    SpuSetReverbModeParam(&r_attr);
    SpuSetReverbDepth(&r_attr);
    SpuSetReverbVoice(SpuOn, SPU_0CH);

    SpuSetReverb(SpuOn);

    /* } else {
        printf("FAIL?\n");
    } */
}

void Sound::load(eastl::string_view filename)
{
    bytes = util::readFile(filename);
    util::FileReader fr{
        .bytes = bytes.data(),
    };

#ifndef _NDEBUG
    eastl::array<unsigned char, 4> header;
    fr.ReadArr(header.data(), 4);
    bool isVag = (header[0] == 'V' && header[1] == 'A' && header[2] == 'G' && header[3] == 'p');
    if (!isVag) {
        printf("%s is not a VAG file\n", filename);
    }
    fr.SkipBytes(14);
#else
    fr.SkipBytes(18);
#endif
    sampleFreq = std::byteswap(fr.GetUInt16());
    printf("%d is not a VAG file\n", sampleFreq);
}

void SoundPlayer::transferVAGToSpu(const Sound& sound, int voicechannel)
{
    vagspuaddr = SpuMalloc(sound.bytes.size());

    SpuSetTransferStartAddr(vagspuaddr);
    SpuWrite((unsigned char*)(sound.bytes.data() + 48), sound.bytes.size() - 48);

    SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

    /* clang-format off */
    spuVoiceAttr.mask = (
        SPU_VOICE_VOLL |
        SPU_VOICE_VOLR |
        SPU_VOICE_PITCH |
        SPU_VOICE_WDSA |
        SPU_VOICE_ADSR_AMODE |
        SPU_VOICE_ADSR_SMODE |
        SPU_VOICE_ADSR_RMODE |
        SPU_VOICE_ADSR_AR |
        SPU_VOICE_ADSR_DR |
        SPU_VOICE_ADSR_SR |
        SPU_VOICE_ADSR_RR |
        SPU_VOICE_ADSR_SL
    );
    /* clang-format on */

    spuVoiceAttr.voice = voicechannel;

    spuVoiceAttr.volume.left = 0x3FFF; // Left volume
    spuVoiceAttr.volume.right = 0x3FFF; // Right volume

    spuVoiceAttr.pitch = (sound.sampleFreq << 12) / 44100; // Pitch
    spuVoiceAttr.addr = vagspuaddr; // Waveform data start address

    spuVoiceAttr.a_mode = SPU_VOICE_LINEARIncN; // Attack curve
    spuVoiceAttr.s_mode = SPU_VOICE_LINEARIncN; // Sustain curve
    spuVoiceAttr.r_mode = SPU_VOICE_LINEARIncN; // Release curve

    spuVoiceAttr.ar = 0x00; // Attack rate
    spuVoiceAttr.dr = 0x00; // Decay rate
    spuVoiceAttr.sr = 0x00; // Sustain rate
    spuVoiceAttr.rr = 0x00; // Release rate
    spuVoiceAttr.sl = 0x00; // Sustain level

    // these attrs are from "reverb" sample
    // spuVoiceAttr.r_mode = SPU_VOICE_LINEARDecN; // Release curve

    SpuSetVoiceAttr(&spuVoiceAttr);
}

void SoundPlayer::playAudio(int voicechannel)
{
    reverbMode = SPU_REV_MODE_HALL;
    r_attr.mode = (SPU_REV_MODE_CLEAR_WA | reverbMode);
    SpuSetReverbModeParam(&r_attr);
    SpuSetReverbDepth(&r_attr);

    SpuSetKey(SpuOn, voicechannel);
}
