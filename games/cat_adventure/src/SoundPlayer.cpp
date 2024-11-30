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

namespace
{
// bits 4-5 Sound RAM Transfer Mode (0=Stop, 1=ManualWrite, 2=DMAwrite, 3=DMAread)
constexpr auto SPU_RAM_TRANSFER_MODE_OFFSET = 4;

enum class SpuRamTransferMode {
    Stop = 0,
    ManualWrite = 1,
    DMAWrite = 2,
    DMARead = 3,
};

constexpr auto SpuDMAWriteMode =
    ((int)SpuRamTransferMode::DMAWrite << SPU_RAM_TRANSFER_MODE_OFFSET);

constexpr auto SpuRamTransferStop = ((int)SpuRamTransferMode::Stop << SPU_RAM_TRANSFER_MODE_OFFSET);

#define SPU_REVERB_START_ADDR HW_U16(0x1f801da2)
#define SPU_REVERB_SETTINGS ((volatile std::uint16_t*)0x1f801dc0)

constexpr auto SPU_REVERB_BIT = 7;
constexpr auto SPU_REVERB_ENABLE = (1 << SPU_REVERB_BIT);

}

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

void SoundPlayer::uploadSound(std::uint32_t SpuAddr, const std::uint8_t* data, std::uint32_t size)
{
    ramsyscall_printf(
        "SPU Upload: (RAM) 0x%08X -> (SPU) 0x%04X, size=%d\n",
        (std::uint32_t)data,
        (SpuAddr >> 3),
        size);
    // Set SPUCNT to "Stop" (and wait until it is applied in SPUSTAT)
    setStopState();

    std::uint32_t bcr = size >> 6;
    if (size & 0x3f) bcr++;
    bcr <<= 16;
    bcr |= 0x10;

    // Set the transfer address
    SPU_RAM_DTA = SpuAddr >> 3;

    // Set SPUCNT to "DMA Write" (and wait until it is applied in SPUSTAT)
    setDMAWriteState();

    SBUS_DEV4_CTRL &= ~0x0f000000;

    DMA_CTRL[DMA_SPU].MADR = (std::uint32_t)data;
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
    SPU_VOL_MAIN_LEFT = 0x3FFF;
    SPU_VOL_MAIN_RIGHT = 0x3FFF;
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

    for (unsigned i = 0; i < 24; i++) {
        resetVoice(i);
    }

    setReverbEnabled();
    setReverbPreset(SpuReverbPreset::StudioLarge);
}

void SoundPlayer::setDMAWriteState()
{
    spuState |= SpuDMAWriteMode;
    setSpuState(spuState);
}

void SoundPlayer::setReverbEnabled()
{
    spuState |= SPU_REVERB_ENABLE;
    setSpuState(spuState);

    SPU_REVERB_LEFT = 0x2850;
    SPU_REVERB_RIGHT = 0x2850;

    SPU_REVERB_EN_LOW = 0xFFFF;
    SPU_REVERB_EN_HIGH = 0xFFFF;
}

void SoundPlayer::setStopState()
{
    static constexpr auto mask = 0b11'111111'1'1'00'1111;
    spuState &= mask;
    setSpuState(spuState);
}

void SoundPlayer::setSpuState(std::uint16_t spuState)
{
    this->spuState = spuState;
    SPU_CTRL = spuState;
    if (spuState & 0b110000) { // SPU mode changed
        while ((SPU_STATUS & 0b110000) != (SPU_CTRL & 0b110000)) {
            // wait until mode is applied
        }
    }

    // TODO: don't do it on each call!
    if (reverbEnabled) {
        SPU_CTRL |= SPU_REVERB_ENABLE;
    } else {
        SPU_CTRL &= ~SPU_REVERB_ENABLE;
        SPU_REVERB_EN_LOW = 0x0000;
        SPU_REVERB_EN_HIGH = 0x0000;
    }
}

void SoundPlayer::uploadSound(std::uint32_t SpuAddr, Sound& sound)
{
    static const auto vagHeaderSize = 48;
    sound.startAddr = SpuAddr;
    if (sound.isVag) {
        uploadSound(SpuAddr, sound.bytes.data() + vagHeaderSize, sound.dataSize);
    } else {
        uploadSound(SpuAddr, sound.bytes.data(), sound.dataSize);
    }
}

static void SPUKeyOn(std::uint32_t voiceBits)
{
    SPU_KEY_ON_LOW = voiceBits;
    SPU_KEY_ON_HIGH = voiceBits >> 16;
}

void SoundPlayer::playSound(int channel, const Sound& sound, std::uint16_t pitch)
{
    setSpuState(0xc000);

    SPU_VOICES[channel].volumeLeft = 0xF00;
    SPU_VOICES[channel].volumeRight = 0xF00;
    SPU_VOICES[channel].sampleStartAddr = sound.startAddr >> 3;
    SPU_VOICES[channel].sampleRate = pitch;
    // SPU_VOICES[channel].sampleRepeatAddr = 0;

    SPUWaitIdle();

    SPUKeyOn(1 << channel);
}

void SoundPlayer::playSound(
    int channel,
    std::uint32_t startAddr,
    std::uint8_t velocity,
    std::uint16_t pitch,
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

void SoundPlayer::setKeyOnOff(std::uint32_t keyOn, std::uint32_t keyOff)
{
    SPU_KEY_ON_LOW = keyOn;
    SPU_KEY_ON_HIGH = keyOn >> 16;

    SPU_KEY_OFF_LOW = keyOff;
    SPU_KEY_OFF_HIGH = keyOff >> 16;
}

namespace
{
std::uint32_t byteswap32(std::uint32_t x)
{
    std::uint32_t y = (x >> 24) & 0xff;
    y |= ((x >> 16) & 0xff) << 8;
    y |= ((x >> 8) & 0xff) << 16;
    y |= (x & 0xff) << 24;
    return y;
}

}

void Sound::load(eastl::string_view filename, const eastl::vector<std::uint8_t>& data)
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

void SoundPlayer::setReverbPreset(SpuReverbPreset preset)
{
    // clang-format off
    static const std::uint16_t reverbSettings[] = {
      /*
      dAPF1  dAPF2  vIIR   vCOMB1 vCOMB2  vCOMB3  vCOMB4  vWALL   ;1F801DC0h..CEh
      vAPF1  vAPF2  mLSAME mRSAME mLCOMB1 mRCOMB1 mLCOMB2 mRCOMB2 ;1F801DD0h..DEh
      dLSAME dRSAME mLDIFF mRDIFF mLCOMB3 mRCOMB3 mLCOMB4 mRCOMB4 ;1F801DE0h..EEh
      dLDIFF dRDIFF mLAPF1 mRAPF1 mLAPF2  mRAPF2  vLIN    vRIN    ;1F801DF0h..FEh
      */
      // Room
      0x007D,0x005B,0x6D80,0x54B8,0xBED0,0x0000,0x0000,0xBA80,
      0x5800,0x5300,0x04D6,0x0333,0x03F0,0x0227,0x0374,0x01EF,
      0x0334,0x01B5,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
      0x0000,0x0000,0x01B4,0x0136,0x00B8,0x005C,0x8000,0x8000,
      // Studio Small
      0x0033,0x0025,0x70F0,0x4FA8,0xBCE0,0x4410,0xC0F0,0x9C00,
      0x5280,0x4EC0,0x03E4,0x031B,0x03A4,0x02AF,0x0372,0x0266,
      0x031C,0x025D,0x025C,0x018E,0x022F,0x0135,0x01D2,0x00B7,
      0x018F,0x00B5,0x00B4,0x0080,0x004C,0x0026,0x8000,0x8000,
      // Studio Medium
      0x00B1,0x007F,0x70F0,0x4FA8,0xBCE0,0x4510,0xBEF0,0xB4C0,
      0x5280,0x4EC0,0x0904,0x076B,0x0824,0x065F,0x07A2,0x0616,
      0x076C,0x05ED,0x05EC,0x042E,0x050F,0x0305,0x0462,0x02B7,
      0x042F,0x0265,0x0264,0x01B2,0x0100,0x0080,0x8000,0x8000,
      // Studio Large 
      0x00E3,0x00A9,0x6F60,0x4FA8,0xBCE0,0x4510,0xBEF0,0xA680,
      0x5680,0x52C0,0x0DFB,0x0B58,0x0D09,0x0A3C,0x0BD9,0x0973,
      0x0B59,0x08DA,0x08D9,0x05E9,0x07EC,0x04B0,0x06EF,0x03D2,
      0x05EA,0x031D,0x031C,0x0238,0x0154,0x00AA,0x8000,0x8000,
      // Hall
      0x01A5,0x0139,0x6000,0x5000,0x4C00,0xB800,0xBC00,0xC000,
      0x6000,0x5C00,0x15BA,0x11BB,0x14C2,0x10BD,0x11BC,0x0DC1,
      0x11C0,0x0DC3,0x0DC0,0x09C1,0x0BC4,0x07C1,0x0A00,0x06CD,
      0x09C2,0x05C1,0x05C0,0x041A,0x0274,0x013A,0x8000,0x8000,
      // Half Echo
      0x0017,0x0013,0x70F0,0x4FA8,0xBCE0,0x4510,0xBEF0,0x8500,
      0x5F80,0x54C0,0x0371,0x02AF,0x02E5,0x01DF,0x02B0,0x01D7,
      0x0358,0x026A,0x01D6,0x011E,0x012D,0x00B1,0x011F,0x0059,
      0x01A0,0x00E3,0x0058,0x0040,0x0028,0x0014,0x8000,0x8000,
      // Space Echo
      0x033D,0x0231,0x7E00,0x5000,0xB400,0xB000,0x4C00,0xB000,
      0x6000,0x5400,0x1ED6,0x1A31,0x1D14,0x183B,0x1BC2,0x16B2,
      0x1A32,0x15EF,0x15EE,0x1055,0x1334,0x0F2D,0x11F6,0x0C5D,
      0x1056,0x0AE1,0x0AE0,0x07A2,0x0464,0x0232,0x8000,0x8000,
      // Chaos Echo
      0x0001,0x0001,0x7FFF,0x7FFF,0x0000,0x0000,0x0000,0x8100,
      0x0000,0x0000,0x1FFF,0x0FFF,0x1005,0x0005,0x0000,0x0000,
      0x1005,0x0005,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
      0x0000,0x0000,0x1004,0x1002,0x0004,0x0002,0x8000,0x8000,
      // Delay
      0x0001,0x0001,0x7FFF,0x7FFF,0x0000,0x0000,0x0000,0x0000,
      0x0000,0x0000,0x1FFF,0x0FFF,0x1005,0x0005,0x0000,0x0000,
      0x1005,0x0005,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
      0x0000,0x0000,0x1004,0x1002,0x0004,0x0002,0x8000,0x8000,
      // Off
      0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
      0x0000,0x0000,0x0001,0x0001,0x0001,0x0001,0x0001,0x0001,
      0x0000,0x0000,0x0001,0x0001,0x0001,0x0001,0x0001,0x0001,
      0x0000,0x0000,0x0001,0x0001,0x0001,0x0001,0x0000,0x0000,
    };
    // clang-format on
    const std::uint32_t reverbSizes[] = {
        0x26C0, // Room
        0x1F40, // Studio Small
        0x4840, // Studio Medium
        0x6FE0, // Studio Large
        0xADE0, // Hall
        0x3C00, // Half Echo
        0xF6C0, // Space Echo
        0x18040, // Chaos Echo
        0x18040, // Delay
        0x10, // Off
    };

    constexpr auto NUM_REVERB_PARAMS = 32;

    const auto presetIndex = (std::uint32_t)preset;
    setReverbSettings(
        eastl::span{&reverbSettings[presetIndex * NUM_REVERB_PARAMS], NUM_REVERB_PARAMS},
        reverbSizes[presetIndex]);

    const auto reverbSize = reverbSizes[presetIndex];
    const auto startAddr = (0x80000 - reverbSize) >> 3;
    SPU_REVERB_START_ADDR = startAddr;
    clearReverbArea(startAddr << 3, reverbSize);
}

void SoundPlayer::setReverbSettings(
    eastl::span<const std::uint16_t> reverbSettings,
    std::uint16_t reverbSize)
{
    for (std::size_t i = 0; i < reverbSettings.size(); ++i) {
        SPU_REVERB_SETTINGS[i] = reverbSettings[i];
    }
}

void SoundPlayer::clearReverbArea(std::uint32_t reverbAreaStartAddr, const uint32_t reverbSize)
{
    // FIXME: not 100% sure about this one, might be broken in some places...
    const auto zeroBlockSize = 1024;
    static eastl::array<uint8_t, zeroBlockSize> zeros = {};
    const auto numDMAs = (reverbSize + zeroBlockSize - 1) / zeroBlockSize;
    for (std::uint32_t i = 0; i < numDMAs; ++i) {
        const auto startAddr = reverbAreaStartAddr + i * zeroBlockSize;
        auto size = (0x80000 - startAddr);
        if (size > zeroBlockSize) {
            size = zeroBlockSize;
        }
        uploadSound(startAddr, zeros.data(), size);
    }
}
