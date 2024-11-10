#include "Font.h"

#include "FileReader.h"

#include <psyqo/kernel.hh>

namespace
{
const std::uint16_t fontMagic = 0x0FAF;
}

void Font::loadFromFile(const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto magic = fr.GetUInt16();
    psyqo::Kernel::assert(magic == fontMagic, "Invalid font magic");

    lineSpacing = fr.GetUInt8();
    ascenderPx = fr.GetUInt8();
    descenderPx = fr.GetUInt8();
    fr.SkipBytes(1); // pad

    // read glyphs
    const auto numGlyphs = fr.GetUInt16();
    for (std::int16_t i = 0; i < numGlyphs; ++i) {
        std::uint16_t codePoint = fr.GetUInt16();
        if (codePoint > 255) {
            // TODO: support non-ASCII
            continue;
        }
        fr.ReadObj(glyphs[codePoint]);
    }
}
