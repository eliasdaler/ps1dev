#pragma once

#include <cassert>
#include <filesystem>
#include <vector>

#include "Color.h"

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
        std::vector<Color16> colors;
    };

    PMode pmode;

    bool hasClut{false};
    std::uint16_t clutDX;
    std::uint16_t clutDY;
    std::uint16_t clutW;
    std::uint16_t clutH;

    std::vector<Clut> cluts;

    std::uint16_t pixDX;
    std::uint16_t pixDY;
    std::uint16_t pixW;
    std::uint16_t pixH;

    // 4bit and 8 bit
    std::vector<std::uint8_t> pixelsIdx;
    // 15bit direct only
    std::vector<Color16> pixels;

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

void writeTimFile(const TimFile& timFile, const std::filesystem::path& path);
