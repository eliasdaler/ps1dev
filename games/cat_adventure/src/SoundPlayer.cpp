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
    do {
        for (unsigned c = 0; c < 2045; c++)
            asm("");
    } while ((SPU_STATUS & 0x07ff) != 0);
}

void SoundPlayer::uploadSound(uint32_t SpuAddr, const uint8_t* data, uint32_t size)
{
    // Set SPUCNT to "Stop" (and wait until it is applied in SPUSTAT)
    setStopState();

    uint32_t bcr = size >> 6;
    if (size & 0x3f) bcr++;
    bcr <<= 16;
    bcr |= 0x10;

    // Set the transfer address
    SPU_RAM_DTA = SpuAddr >> 3;

    // Set SPUCNT to "DMA Write" (and wait until it is applied in SPUSTAT)
    spuState = (spuState & ~0x0030) | 0x0020;
    setSpuState(spuState);
    // setDMAWriteState();

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
}

void SoundPlayer::setDMAWriteState()
{
    static constexpr auto mask = 0b11'111111'1'1'10'1111;
    spuState &= mask;
    setSpuState(spuState);
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
}

void SoundPlayer::uploadSound(uint32_t SpuAddr, const Sound& sound)
{
    static const auto vagHeaderSize = 0x30;
    sound.startAddr = SpuAddr;
    uploadSound(SpuAddr, sound.bytes.data() + vagHeaderSize, sound.dataSize);
}

static void SPUKeyOn(uint32_t voiceBits)
{
    SPU_KEY_ON_LOW = voiceBits;
    SPU_KEY_ON_HIGH = voiceBits >> 16;
}

void SoundPlayer::playSound(int channel, const Sound& sound, uint16_t pitch)
{
    setSpuState(0xc000);

    SPU_VOICES[channel].volumeLeft = 0xFF0;
    SPU_VOICES[channel].volumeRight = 0xF00;
    SPU_VOICES[channel].sampleStartAddr = sound.startAddr >> 3;
    SPUWaitIdle();
    SPUKeyOn(1 << channel);
    SPU_VOICES[channel].sampleRepeatAddr = 0;
    SPU_VOICES[channel].sampleRate = pitch;
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
