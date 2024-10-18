#pragma once

#include <cstdint>

#include <EASTL/vector.h>

using Color16 = std::uint16_t;

struct TimFile {
    static constexpr std::uint32_t MAGIC = 0x00000010;

    enum class PMode {
        Clut4Bit,
        Clut8Bit,
        Direct15Bit,
        Direct24Bit,
        Mixed,
    };

    struct Clut {
        eastl::vector<Color16> colors;
    };

    PMode pmode;

    bool hasClut{false};
    std::int16_t clutDX;
    std::int16_t clutDY;
    std::int16_t clutW;
    std::int16_t clutH;

    eastl::vector<Clut> cluts;

    std::uint16_t pixDX;
    std::uint16_t pixDY;
    std::uint16_t pixW;
    std::uint16_t pixH;

    // 4bit and 8 bit
    eastl::vector<std::uint8_t> pixelsIdx;
    // 15bit direct only
    eastl::vector<Color16> pixels;

    static std::size_t getNumColorsInClut(TimFile::PMode pmode)
    {
        if (pmode == TimFile::PMode::Clut4Bit) {
            return 16;
        }
        if (pmode == TimFile::PMode::Clut8Bit) {
            return 256;
        }
        return 0;
    }
};

TimFile readTimFile(const eastl::vector<uint8_t>& data);
