#include "TimFileRead.h"

#include "FileReader.h"
#include "TimFile.h"

namespace
{
template<std::size_t N>
TimFile::Clut readClut(FileReader& fr)
{
    TimFile::Clut clut;
    clut.colors.resize(N);
    for (int i = 0; i < N; ++i) {
        auto clutEntry = fr.GetUInt16();
        clut.colors[i] = clutEntry;
    }
    return clut;
}

std::pair<std::uint16_t, std::uint16_t> getXY16(std::uint32_t xy)
{
    return {xy & 0xFFFF, xy >> 16};
}

}

TimFile readTimFile(const std::filesystem::path& path)
{
    std::cout << "Reading TIM from " << path << std::endl;
    const auto contents = readFile(path);

    FileReader fr{.bytes = std::span{contents}};

    // Bits 0-7: ID value is 0x10.
    // Bits 8-15: Version number (0x00).
    // Bits 16-31: All zero.
    const auto magic = fr.GetUInt32();
    assert(magic == TimFile::MAGIC && "Invalid TIM magic");

    const auto flag = fr.GetUInt32();

    TimFile timFile;

    // Bits 0-2 (PMODE):
    // 0 - 4-bit CLUT
    // 1 - 8-bit CLUT
    // 2 - 15-bit direct
    // 3 - 24-bit direct
    // 4 - Mixed
    timFile.pmode = static_cast<TimFile::PMode>(flag & 0b11);
    if (timFile.pmode == TimFile::PMode::Clut4Bit) {
        std::cout << "PMode: 4-bit CLUT\n";
    } else if (timFile.pmode == TimFile::PMode::Clut8Bit) {
        std::cout << "PMode: 8-bit CLUT\n";
    } else if (timFile.pmode == TimFile::PMode::Direct15Bit) {
        std::cout << "PMode: 24-bit CLUT\n";
    } else {
        std::cout << "PMode: not supported yet\n";
        std::exit(1);
    }

    // Bit 3 (CF)
    timFile.hasClut = ((flag & 0b1000) != 0);
    std::cout << "has CLUT: " << timFile.hasClut << std::endl;

    if (timFile.hasClut) { // CLUT
        const auto clutBNum = fr.GetUInt32();
        std::cout << "CLUT bnum: " << clutBNum << std::endl;

        // 31     0
        // [DY, DX]
        const auto [clutDX, clutDY] = getXY16(fr.GetUInt32());
        timFile.clutDX = clutDX;
        timFile.clutDY = clutDY;
        std::cout << "CLUT DX: " << clutDX << ", CLUT DY: " << clutDY << std::endl;

        // 31     0
        // [W,   H]
        const auto [clutW, clutH] = getXY16(fr.GetUInt32());
        timFile.clutW = clutW;
        timFile.clutH = clutH;
        std::cout << "CLUT W: " << clutW << ", CLUT H: " << clutH << std::endl;

        // TODO: handle CLUTs positioned side by side?
        timFile.cluts.reserve(timFile.clutH);
        const auto clutNumColors = TimFile::getNumColorsInClut(timFile.pmode);
        for (int i = 0; i < timFile.clutH; ++i) {
            TimFile::Clut clut;
            clut.colors.resize(clutNumColors);
            for (int i = 0; i < clutNumColors; ++i) {
                clut.colors[i] = fr.GetUInt16();
            }
            timFile.cluts.push_back(std::move(clut));
        }
    }

    // pixel data
    {
        const auto pixBNum = fr.GetUInt32();
        std::cout << "pixels bnum: " << pixBNum << std::endl;

        // 31     0
        // [DY, DX]
        const auto [pixDX, pixDY] = getXY16(fr.GetUInt32());
        timFile.pixDX = pixDX;
        timFile.pixDY = pixDY;
        std::cout << "pix DX: " << pixDX << ", pix DY: " << pixDY << std::endl;

        // 31     0
        // [W,   H]
        const auto [pixW, pixH] = getXY16(fr.GetUInt32());
        timFile.pixW = pixW;
        timFile.pixH = pixH;
        std::cout << "pix W: " << pixW << ", pix H: " << pixH << std::endl;

        auto numPixels = pixW * pixH;
        if (timFile.pmode == TimFile::PMode::Clut4Bit) {
            numPixels *= 4;
        } else if (timFile.pmode == TimFile::PMode::Clut8Bit) {
            numPixels *= 2;
        }

        if (timFile.hasClut) {
            timFile.pixelsIdx.resize(numPixels);
        } else {
            timFile.pixels.resize(numPixels);
        }

        for (int i = 0; i < pixW * pixH; ++i) {
            const auto pd = fr.GetUInt16();
            if (timFile.pmode == TimFile::PMode::Clut4Bit) {
                timFile.pixelsIdx[i * 4 + 0] = static_cast<std::uint8_t>((pd & 0x000F) >> 0);
                timFile.pixelsIdx[i * 4 + 1] = static_cast<std::uint8_t>((pd & 0x00F0) >> 4);
                timFile.pixelsIdx[i * 4 + 2] = static_cast<std::uint8_t>((pd & 0x0F00) >> 8);
                timFile.pixelsIdx[i * 4 + 3] = static_cast<std::uint8_t>((pd & 0xF000) >> 12);
                if (i < 10) {
                    std::cout << std::format("{:016b}\n", pd);
                    std::cout << (int)timFile.pixelsIdx[i * 4 + 0] << " "
                              << (int)timFile.pixelsIdx[i * 4 + 1] << " "
                              << (int)timFile.pixelsIdx[i * 4 + 2] << " "
                              << (int)timFile.pixelsIdx[i * 4 + 3] << std::endl;
                }
            } else if (timFile.pmode == TimFile::PMode::Clut8Bit) {
                timFile.pixelsIdx[i * 2 + 0] = static_cast<std::uint8_t>(pd);
                timFile.pixelsIdx[i * 2 + 1] = static_cast<std::uint8_t>(pd >> 8);
            } else if (timFile.pmode == TimFile::PMode::Direct15Bit) {
                timFile.pixels[i] = pd;
            }
        }
    }
    return timFile;
}
