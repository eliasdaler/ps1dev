#include "TimFileWrite.h"

#include "TimFile.h"

#include <FsUtil.h>
#include <fstream>

namespace
{
std::uint32_t toXY32(std::uint16_t x, std::uint16_t y)
{
    return (y << 16) | x;
}
}

void writeTimFile(const TimFile& timFile, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);

    // magic
    fsutil::binaryWrite(file, TimFile::MAGIC);

    // flag
    auto flag = static_cast<std::uint32_t>(timFile.pmode);
    if (timFile.hasClut) {
        flag |= 0x8; // 3rd bit
    }
    fsutil::binaryWrite(file, flag);

    // clut
    assert(
        (timFile.pmode == TimFile::PMode::Clut4Bit || timFile.pmode == TimFile::PMode::Clut8Bit ||
         timFile.pmode == TimFile::PMode::Direct15Bit) &&
        "Unsupported PMode");

    if (timFile.hasClut) {
        const auto clutNumColors = TimFile::getNumColorsInClut(timFile.pmode);
        std::uint32_t bnum = 12u + timFile.cluts.size() * clutNumColors * sizeof(Color16);

        fsutil::binaryWrite(file, bnum);
        fsutil::binaryWrite(file, toXY32(timFile.clutDX, timFile.clutDY));
        fsutil::binaryWrite(file, toXY32(timFile.clutW, timFile.clutH));

        if (timFile.pmode == TimFile::PMode::Clut4Bit ||
            timFile.pmode == TimFile::PMode::Clut8Bit) {
            for (const auto& clut : timFile.cluts) {
                for (const auto& c : clut.colors) {
                    fsutil::binaryWrite(file, c);
                }
            }
        }
    }

    { // pixel data
        // bnum + DXDY + WH + pixels size
        std::uint32_t bnum = 12u + (timFile.pixW * timFile.pixH) * 2;

        fsutil::binaryWrite(file, bnum);
        fsutil::binaryWrite(file, toXY32(timFile.pixDX, timFile.pixDY));
        fsutil::binaryWrite(file, toXY32(timFile.pixW, timFile.pixH));

        // how many pixels fit in one element
        auto pixelsPerElem = [](TimFile::PMode pmode) {
            switch (pmode) {
            case TimFile::PMode::Direct15Bit:
                return 1;
            case TimFile::PMode::Clut8Bit:
                return 2;
            case TimFile::PMode::Clut4Bit:
                return 4;
            default:
                assert(false);
                break;
            }
        }(timFile.pmode);

        // Direct15Bit - just write file.pixels[i]
        // 8Bit - i0 | i1
        // 4Bit - i0 | i1 | i2 | i3
        auto convertPixels = [](const TimFile& file, std::size_t i) -> std::uint16_t {
            switch (file.pmode) {
            case TimFile::PMode::Direct15Bit:
                return file.pixels[i];
            case TimFile::PMode::Clut8Bit:
                return (file.pixelsIdx[i + 1] << 8) | (file.pixelsIdx[i + 0] << 0);
            case TimFile::PMode::Clut4Bit:
                return (file.pixelsIdx[i + 3] << 12) | (file.pixelsIdx[i + 2] << 8) |
                       (file.pixelsIdx[i + 1] << 4) | (file.pixelsIdx[i + 0] << 0);
            default:
                assert(false);
                break;
            }
        };

        for (std::size_t i = 0; i < timFile.pixW * timFile.pixH; ++i) {
            fsutil::binaryWrite(file, convertPixels(timFile, i * pixelsPerElem));
        }
    }
}
