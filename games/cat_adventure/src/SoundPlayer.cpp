#include "SoundPlayer.h"

#include <EASTL/array.h>
#include <bit>
#include <stdio.h>

#include "FileReader.h"

#include <common/syscalls/syscalls.h>

#include <stdint.h>

#include "common/hardware/dma.h"
#include "common/hardware/spu.h"

extern uint8_t _binary_loop_adpcm_start[];
extern uint8_t _binary_loop_adpcm_end[];

static void SPUResetVoice(int voiceID)
{
    SPU_VOICES[voiceID].volumeLeft = 0;
    SPU_VOICES[voiceID].volumeRight = 0;
    SPU_VOICES[voiceID].sampleRate = 0;
    SPU_VOICES[voiceID].sampleStartAddr = 0;
    SPU_VOICES[voiceID].ad = 0x000f;
    SPU_VOICES[voiceID].currentVolume = 0;
    SPU_VOICES[voiceID].sampleRepeatAddr = 0;
    SPU_VOICES[voiceID].sr = 0x0000;
}

static void SPUWaitIdle()
{
    do {
        for (unsigned c = 0; c < 2045; c++)
            asm("");
    } while ((SPU_STATUS & 0x07ff) != 0);
}

static void SPUUploadInstruments(uint32_t SpuAddr, const uint8_t* data, uint32_t size)
{
    DPCR |= 0x000b0000;

    uint32_t bcr = size >> 6;
    if (size & 0x3f) bcr++;
    bcr <<= 16;
    bcr |= 0x10;

    SPU_RAM_DTA = SpuAddr >> 3;
    SPU_CTRL = (SPU_CTRL & ~0x0030) | 0x0020;
    while ((SPU_CTRL & 0x0030) != 0x0020) {
        // wait
    }
    SBUS_DEV4_CTRL &= ~0x0f000000;
    DMA_CTRL[DMA_SPU].MADR = (uint32_t)data;
    DMA_CTRL[DMA_SPU].BCR = bcr;
    DMA_CTRL[DMA_SPU].CHCR = 0x01000201;

    while ((DMA_CTRL[DMA_SPU].CHCR & 0x01000000) != 0) {
        // wait
    }
}

void spuInit()
{
    DPCR |= 0x000b0000;
    SPU_VOL_MAIN_LEFT = 0x3800;
    SPU_VOL_MAIN_RIGHT = 0x3800;
    SPU_CTRL = 0;
    SPU_KEY_ON_LOW = 0;
    SPU_KEY_ON_HIGH = 0;
    SPU_KEY_OFF_LOW = 0xffff;
    SPU_KEY_OFF_HIGH = 0xffff;
    SPU_RAM_DTC = 4;
    SPU_VOL_CD_LEFT = 0;
    SPU_VOL_CD_RIGHT = 0;
    SPU_PITCH_MOD_LOW = 0;
    SPU_PITCH_MOD_HIGH = 0;
    SPU_NOISE_EN_LOW = 0;
    SPU_NOISE_EN_HIGH = 0;
    SPU_REVERB_EN_LOW = 0;
    SPU_REVERB_EN_HIGH = 0;
    SPU_VOL_EXT_LEFT = 0;
    SPU_VOL_EXT_RIGHT = 0;
    SPU_CTRL = 0x8000;

    for (unsigned i = 0; i < 24; i++)
        SPUResetVoice(i);
}

void uploadSound(uint32_t SpuAddr, Sound& sound)
{
    static const auto vagHeaderSize = 0x30;
    SPUUploadInstruments(SpuAddr, sound.bytes.data() + vagHeaderSize, sound.dataSize);
}

void playLoop(Sound& sound, uint16_t pitch)
{
    // uploadSound(0x1010, sound);
    // SPUUploadInstruments(0x1010, sound.bytes.data(), sound.bytes.size());

    SPU_CTRL = 0xc000;

    SPU_VOICES[0].volumeLeft = 0xFF0;
    SPU_VOICES[0].volumeRight = 0xF00;
    SPU_VOICES[0].sampleStartAddr = 0x1010 >> 3;
    SPUWaitIdle();
    SPU_KEY_ON_LOW = 1;
    SPU_KEY_ON_HIGH = 0;
    SPU_VOICES[0].sampleRate = pitch;
    // SPU_VOICES[0].sampleRate = 2048;
}

void SoundPlayer::init()
{
    // SpuInit();
    // SpuInitMalloc(SOUND_MALLOC_MAX, spumallocrec);

    // Set common attributes (master volume left/right, CD volume, etc.)
    /* spucommonattr.mask =
        (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR |
         SPU_COMMON_CDMIX); */

    // Master volume (left) - should be between 0x0000 and 0x3FFF
    // spucommonattr.mvol.left = 0x3FFF;
    // spucommonattr.mvol.right = 0x3FFF; // Master volume (right)

    // CD volume (left)     -> should be between 0x0000 and 0x7FFF
    // spucommonattr.cd.volume.left = 0x7FFF;
    // spucommonattr.cd.volume.right = 0x7FFF; // CD volume (right)

    // spucommonattr.cd.mix = SPU_ON; // Enable CD input on

    // SpuSetCommonAttr(&spucommonattr);
    // SpuSetTransferMode(SpuTransByDMA);

    initReverb();
}

void SoundPlayer::initReverb()
{
    // long mode = SPU_REV_MODE_ROOM | SPU_REV_MODE_STUDIO_A | SPU_REV_MODE_HALL |
    // SPU_REV_MODE_SPACE;

    // if (SpuReserveReverbWorkArea(1) == 1) {
    // reverbMode = SPU_REV_MODE_OFF;

    // r_attr.mask = (SPU_REV_MODE | SPU_REV_DEPTHL | SPU_REV_DEPTHR);
    // r_attr.mode = (SPU_REV_MODE_CLEAR_WA | reverbMode);
    // r_attr.depth.left = 0x3fff;
    // r_attr.depth.right = 0x3fff;

    // SpuSetReverbModeParam(&r_attr);
    // SpuSetReverbDepth(&r_attr);
    // SpuSetReverbVoice(SpuOn, SPU_0CH);

    // SpuSetReverb(SpuOn);

    /* } else {
        printf("FAIL?\n");
    } */
}

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

}

void Sound::load(eastl::string_view filename, const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

#ifndef _NDEBUG
    eastl::array<unsigned char, 4> header;
    fr.ReadArr(header.data(), 4);
    bool isVag = (header[0] == 'V' && header[1] == 'A' && header[2] == 'G' && header[3] == 'p');
    if (!isVag) {
        ramsyscall_printf("%s is not a VAG file\n", filename);
    }
    fr.SkipBytes(8);
#else
    fr.SkipBytes(12);
#endif

    dataSize = byteswap32(fr.GetUInt32());
    sampleFreq = byteswap32(fr.GetUInt32());

    ramsyscall_printf("vag sample freq: %d, data size = %d\n", (int)sampleFreq, (int)dataSize);

    bytes = data;
}

void SoundPlayer::transferVAGToSpu(const Sound& sound, int voicechannel)
{
    // vagspuaddr = SpuMalloc(sound.bytes.size());

    // SpuSetTransferStartAddr(vagspuaddr);
    // SpuWrite((unsigned char*)(sound.bytes.data() + 48), sound.bytes.size() - 48);

    // SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

    /* clang-format off */
    /* spuVoiceAttr.mask = (
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
    ); */
    /* clang-format on */

    // spuVoiceAttr.voice = voicechannel;

    // spuVoiceAttr.volume.left = 0x3FFF; // Left volume
    // spuVoiceAttr.volume.right = 0x3FFF; // Right volume

    // spuVoiceAttr.pitch = (sound.sampleFreq << 12) / 44100; // Pitch
    // spuVoiceAttr.addr = vagspuaddr; // Waveform data start address

    // spuVoiceAttr.a_mode = SPU_VOICE_LINEARIncN; // Attack curve
    // spuVoiceAttr.s_mode = SPU_VOICE_LINEARIncN; // Sustain curve
    // spuVoiceAttr.r_mode = SPU_VOICE_LINEARIncN; // Release curve

    // spuVoiceAttr.ar = 0x00; // Attack rate
    // spuVoiceAttr.dr = 0x00; // Decay rate
    // spuVoiceAttr.sr = 0x00; // Sustain rate
    // spuVoiceAttr.rr = 0x00; // Release rate
    // spuVoiceAttr.sl = 0x00; // Sustain level

    // these attrs are from "reverb" sample
    // spuVoiceAttr.r_mode = SPU_VOICE_LINEARDecN; // Release curve

    // SpuSetVoiceAttr(&spuVoiceAttr);
}

void SoundPlayer::playAudio(int voicechannel)
{
    // reverbMode = SPU_REV_MODE_HALL;
    // r_attr.mode = (SPU_REV_MODE_CLEAR_WA | reverbMode);
    // SpuSetReverbModeParam(&r_attr);
    // SpuSetReverbDepth(&r_attr);

    // SpuSetKey(SpuOn, voicechannel);
}
