#include "TimFile.h"

#include "FileReader.h"

#include <psyqo/kernel.hh>

#include <common/syscalls/syscalls.h>

namespace
{
/* template<std::size_t N>
TimFile::Clut readClut(FileReader& fr)
{
    TimFile::Clut clut;
    clut.colors.resize(N);
    for (int i = 0; i < N; ++i) {
        auto clutEntry = fr.GetUInt16();
        clut.colors[i] = clutEntry;
    }
    return clut;
} */

eastl::pair<std::uint16_t, std::uint16_t> getXY16(std::uint32_t xy)
{
    return {xy & 0xFFFF, xy >> 16};
}

}

TimFile readTimFile(const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

    // Bits 0-7: ID value is 0x10.
    // Bits 8-15: Version number (0x00).
    // Bits 16-31: All zero.
    const auto magic = fr.GetUInt32();
    psyqo::Kernel::assert(magic == TimFile::MAGIC, "Invalid TIM magic");

    const auto flag = fr.GetUInt32();

    TimFile tim;

    // Bits 0-2 (PMODE):
    // 0 - 4-bit CLUT
    // 1 - 8-bit CLUT
    // 2 - 15-bit direct
    // 3 - 24-bit direct
    // 4 - Mixed
    tim.pmode = static_cast<TimFile::PMode>(flag & 0b11);
    if (tim.pmode == TimFile::PMode::Clut4Bit) {
        ramsyscall_printf("PMode: 4-bit CLUT\n");
    } else if (tim.pmode == TimFile::PMode::Clut8Bit) {
        ramsyscall_printf("PMode: 8-bit CLUT\n");
    } else if (tim.pmode == TimFile::PMode::Direct15Bit) {
        ramsyscall_printf("PMode: Direct 15-bit CLUT\n");
        psyqo::Kernel::assert(false, "TODO");
    } else {
        ramsyscall_printf("PMode: not supported yet\n");
        psyqo::Kernel::assert(false, "TODO");
    }

    // Bit 3 (CF)
    tim.hasClut = ((flag & 0b1000) != 0);
    ramsyscall_printf("has CLUT: %d\n", (int)tim.hasClut);

    if (tim.hasClut) { // CLUT
        const auto clutBNum = fr.GetUInt32();
        ramsyscall_printf("CLUT bnum: %d\n", (int)clutBNum);

        // 31     0
        // [DY, DX]
        const auto [clutDX, clutDY] = getXY16(fr.GetUInt32());
        tim.clutDX = clutDX;
        tim.clutDY = clutDY;
        ramsyscall_printf("CLUT DX: %d, DY: %d\n", clutDX, clutDY);

        // 31     0
        // [W,   H]
        const auto [clutW, clutH] = getXY16(fr.GetUInt32());
        tim.clutW = clutW;
        tim.clutH = clutH;
        ramsyscall_printf("CLUT W: %d, H: %d\n", clutW, clutH);

        // TODO: handle CLUTs positioned side by side?
        tim.cluts.reserve(tim.clutH);

        const auto clutNumColors = TimFile::getNumColorsInClut(tim.pmode);
        for (int i = 0; i < tim.clutH; ++i) {
            TimFile::Clut clut;
            clut.colors.resize(clutNumColors);
            for (int i = 0; i < clutNumColors; ++i) {
                clut.colors[i] = fr.GetUInt16();
            }
            tim.cluts.push_back(eastl::move(clut));
        }
    }

    // pixel data
    {
        const auto pixBNum = fr.GetUInt32();
        ramsyscall_printf("pixels bnum: %d\n", (int)pixBNum);

        // 31     0
        // [DY, DX]
        const auto [pixDX, pixDY] = getXY16(fr.GetUInt32());
        tim.pixDX = pixDX;
        tim.pixDY = pixDY;
        ramsyscall_printf("CLUT DX: %d, DY: %d\n", pixDX, pixDY);

        // 31     0
        // [W,   H]
        const auto [pixW, pixH] = getXY16(fr.GetUInt32());
        tim.pixW = pixW;
        tim.pixH = pixH;
        ramsyscall_printf("pix W: %d, H: %d\n", pixW, pixH);

        auto numPixels = pixW * pixH;
        if (tim.pmode == TimFile::PMode::Clut4Bit) {
            numPixels *= 4;
        } else if (tim.pmode == TimFile::PMode::Clut8Bit) {
            numPixels *= 2;
        }

        if (tim.hasClut) {
            tim.pixelsIdx.resize(numPixels);
        } else {
            tim.pixels.resize(numPixels);
        }

        for (int i = 0; i < pixW * pixH; ++i) {
            const auto pd = fr.GetUInt16();
            if (tim.pmode == TimFile::PMode::Clut4Bit) {
                tim.pixelsIdx[i * 4 + 0] = static_cast<std::uint8_t>((pd & 0x000F) >> 0);
                tim.pixelsIdx[i * 4 + 1] = static_cast<std::uint8_t>((pd & 0x00F0) >> 4);
                tim.pixelsIdx[i * 4 + 2] = static_cast<std::uint8_t>((pd & 0x0F00) >> 8);
                tim.pixelsIdx[i * 4 + 3] = static_cast<std::uint8_t>((pd & 0xF000) >> 12);
            } else if (tim.pmode == TimFile::PMode::Clut8Bit) {
                tim.pixelsIdx[i * 2 + 0] = static_cast<std::uint8_t>(pd);
                tim.pixelsIdx[i * 2 + 1] = static_cast<std::uint8_t>(pd >> 8);
            } else if (tim.pmode == TimFile::PMode::Direct15Bit) {
                tim.pixels[i] = pd;
            }
        }
    }

    return tim;
}
