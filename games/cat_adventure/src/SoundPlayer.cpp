#include "SoundPlayer.h"

#include <EASTL/array.h>
#include <bit>
#include <stdio.h>

#include "FileReader.h"

#include <common/syscalls/syscalls.h>

#include <stdint.h>

#include "common/hardware/dma.h"
#include "common/hardware/spu.h"

#include "VabFile.h"

#include <psyqo/fixed-point.hh>

bool SoundPlayer::reverbEnabled = true;

void SoundPlayer::resetVoice(int voiceID)
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
    while ((SPU_STATUS & 0x07ff) != 0) {
        // wait
    }
}

void SoundPlayer::uploadSound(uint32_t SpuAddr, const uint8_t* data, uint32_t size)
{
    ramsyscall_printf("SPU Upload: 0x%08X -> 0x%08X, size=%d\n", (uint32_t)data, SpuAddr, size);
    // Set SPUCNT to "Stop" (and wait until it is applied in SPUSTAT)
    setStopState();

    uint32_t bcr = size >> 6;
    if (size & 0x3f) bcr++;
    bcr <<= 16;
    bcr |= 0x10;

    // Set the transfer address
    SPU_RAM_DTA = SpuAddr >> 3;

    // Set SPUCNT to "DMA Write" (and wait until it is applied in SPUSTAT)
    // spuState = (spuState & ~0x0030) | 0x0020;
    // setSpuState(spuState);
    setDMAWriteState();

    SBUS_DEV4_CTRL &= ~0x0f000000;

    DMA_CTRL[DMA_SPU].MADR = (uint32_t)data;
    DMA_CTRL[DMA_SPU].BCR = bcr;
    // Start DMA4 at CPU Side (blocksize=10h, control=01000201h)
    DMA_CTRL[DMA_SPU].CHCR = 0x01000201;

    // Wait until DMA4 finishes (at CPU side)
    while ((DMA_CTRL[DMA_SPU].CHCR & 0x01000000) != 0) {
        // wait
    }
}

void SoundPlayer::init()
{
    DPCR |= 0x000b0000; // WHY?
    SPU_VOL_MAIN_LEFT = 0x3800;
    SPU_VOL_MAIN_RIGHT = 0x3800;
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

    // [spu enable][unmute_spu][6bits for noise][reverb][irq][2 bits for mode][4 bits for ext/CD
    // audio]
    spuState = 0b11'000000'1'1'00'0000;
    setSpuState(spuState);

    for (unsigned i = 0; i < 24; i++)
        resetVoice(i);

    setReverbEnabled();

    // clang-format off
    static const uint16_t reverbBits[] = {
      0x00E3,0x00A9,0x6F60,0x4FA8,0xBCE0,0x4510,0xBEF0,0xA680,
      0x5680,0x52C0,0x0DFB,0x0B58,0x0D09,0x0A3C,0x0BD9,0x0973,
      0x0B59,0x08DA,0x08D9,0x05E9,0x07EC,0x04B0,0x06EF,0x03D2,
      0x05EA,0x031D,0x031C,0x0238,0x0154,0x00AA,0x8000,0x8000,

  /* 0x033D,0x0231,0x7E00,0x5000,0xB400,0xB000,0x4C00,0xB000,
  0x6000,0x5400,0x1ED6,0x1A31,0x1D14,0x183B,0x1BC2,0x16B2,
  0x1A32,0x15EF,0x15EE,0x1055,0x1334,0x0F2D,0x11F6,0x0C5D,
  0x1056,0x0AE1,0x0AE0,0x07A2,0x0464,0x0232,0x8000,0x8000, */
    };
    for (int i = 0; i < 32; ++i) {
    *(volatile uint16_t*)(0x1F801DC0 + i * 2)  = reverbBits[i];
    }
    // clang-format on
}

void SoundPlayer::setDMAWriteState()
{
    static constexpr auto mask = 0b00'000000'0'0'10'0000;
    spuState |= mask;
    setSpuState(spuState);
}

void SoundPlayer::setReverbEnabled()
{
    static constexpr auto mask = 0b00'000001'1'0'00'0000;
    spuState |= mask;
    setSpuState(spuState);

    SPU_REVERB_LEFT = 0x2850;
    SPU_REVERB_RIGHT = 0x2850;

    *(volatile uint16_t*)(0x1F801DA2) = 0xf104;
    *(volatile uint32_t*)(0x1F801D98) = 0xFFFFFFFF;
}

void SoundPlayer::setStopState()
{
    static constexpr auto mask = 0b11'111111'1'1'00'1111;
    spuState &= mask;
    setSpuState(spuState);
}

void SoundPlayer::setSpuState(int spuState)
{
    this->spuState = spuState;
    SPU_CTRL = spuState;
    if (spuState & 0b110000) { // SPU mode changed
        while ((SPU_STATUS & 0b11111) != (SPU_CTRL & 0b11111)) {
            // wait until mode is applied
        }
    }

    // TODO: don't do it on each call!
    if (reverbEnabled) {
        *(volatile unsigned short*)0x1F801DAA |= (1 << 7);
    } else {
        *(volatile unsigned short*)0x1F801DAA &= ~(1 << 7);
    }
}

void SoundPlayer::uploadSound(uint32_t SpuAddr, Sound& sound)
{
    static const auto vagHeaderSize = 48;
    sound.startAddr = SpuAddr;
    if (sound.isVag) {
        uploadSound(SpuAddr, sound.bytes.data() + vagHeaderSize, sound.dataSize);
    } else {
        uploadSound(SpuAddr, sound.bytes.data(), sound.dataSize);
    }
}

static void SPUKeyOn(uint32_t voiceBits)
{
    SPU_KEY_ON_LOW = voiceBits;
    SPU_KEY_ON_HIGH = voiceBits >> 16;
}

void SoundPlayer::playSound(int channel, const Sound& sound, uint16_t pitch)
{
    setSpuState(0xc000);

    SPU_VOICES[channel].volumeLeft = 0xF00;
    SPU_VOICES[channel].volumeRight = 0xF00;
    SPU_VOICES[channel].sampleStartAddr = sound.startAddr >> 3;
    // SPU_VOICES[channel].sampleRepeatAddr = 0;
    SPUWaitIdle();
    SPUKeyOn(1 << channel);
    SPU_VOICES[channel].sampleRate = pitch;
}

void SoundPlayer::playSound(
    int channel,
    uint32_t startAddr,
    uint8_t velocity,
    uint16_t pitch,
    const ToneAttribute& toneAttrib)
{
    setSpuState(0xc000);

    auto volLeft = 0x4F * ((toneAttrib.pan) / 2);
    auto volRight = 0x4F * ((128 - toneAttrib.pan) / 2);

    psyqo::FixedPoint<> volL, volR;

    volL.value = volLeft;
    volR.value = volRight;

    psyqo::FixedPoint<> velo;
    velo.value = velocity << 5; // 128 -> 4096

    psyqo::FixedPoint<> volMul;
    volMul.value = toneAttrib.vol << 5; // 128 -> 4096

    volL *= velo * volMul;
    volR *= velo * volMul;

    // TODO: proper velocity calculation
    SPU_VOICES[channel].volumeLeft = volL.value;
    SPU_VOICES[channel].volumeRight = volR.value;

    SPU_VOICES[channel].ad = toneAttrib.ad;
    SPU_VOICES[channel].sr = toneAttrib.sr;
    SPU_VOICES[channel].sampleStartAddr = startAddr >> 3;
    SPU_VOICES[channel].sampleRate = pitch;
    // SPUWaitIdle();
    // SPUKeyOn(1 << channel);
}

void SoundPlayer::setKeyOnOff(int keyOn, int keyOff)
{
    SPU_KEY_ON_LOW = keyOn;
    SPU_KEY_ON_HIGH = keyOn >> 16;

    SPU_KEY_OFF_LOW = keyOff;
    SPU_KEY_OFF_HIGH = keyOff >> 16;
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

    const auto header = fr.GetUInt32();
    isVag = (header == 0x70474156);

    if (!isVag) { // raw adpcm
        ramsyscall_printf("%s is not a VAG file, header: %08X\n", filename, (int)header);
        bytes = data;
        sampleFreq = 44100;
        dataSize = data.size();
        return;
    }

    fr.SkipBytes(8);

    dataSize = byteswap32(fr.GetUInt32());
    sampleFreq = byteswap32(fr.GetUInt32());

    ramsyscall_printf("vag sample freq: %d, data size = %d\n", sampleFreq, (int)dataSize);

    bytes = data;
}
