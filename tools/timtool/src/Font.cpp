#include "Font.h"

#include <iostream>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <FsUtil.h>
#include <fstream>

static constexpr int GLYPH_ATLAS_SIZE = 256;

bool Font::load(const std::filesystem::path& path, int size, bool antialiasing)
{
    // only load ASCII by default
    std::unordered_set<std::uint32_t> neededCodePoints;
    for (std::uint32_t i = 0; i < 255; ++i) {
        neededCodePoints.insert(i);
    }
    return load(path, size, neededCodePoints, antialiasing);
}

bool Font::load(
    const std::filesystem::path& path,
    int size,
    const std::unordered_set<std::uint32_t>& neededCodePoints,
    bool antialiasing)
{
    std::cout << "Loading font: " << path << ", size=" << size
              << ", num glyphs to load=" << neededCodePoints.size() << std::endl;

    this->size = size;

    FT_Library ft;
    if (const auto err = FT_Init_FreeType(&ft)) {
        std::cout << "Failed to init FreeType Library: " << FT_Error_String(err) << std::endl;
        return false;
    }

    FT_Face face;
    if (const auto err = FT_New_Face(ft, path.string().c_str(), 0, &face)) {
        std::cout << "Failed to load font: " << FT_Error_String(err) << std::endl;
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, size);

    // because metrics.height etc. is stored as 1/64th of pixels
    lineSpacing = face->size->metrics.height / 64.f;
    ascenderPx = face->size->metrics.ascender / 64.f;
    descenderPx = face->size->metrics.descender / 64.f;

    // TODO: allow to specify atlas size?
    int aw = GLYPH_ATLAS_SIZE;
    int ah = GLYPH_ATLAS_SIZE;
    atlasSize.x = GLYPH_ATLAS_SIZE;
    atlasSize.y = GLYPH_ATLAS_SIZE;
    atlasBitmapData.resize(aw * ah);

    int pen_x = 0;
    int pen_y = 0;

    int maxHeightInRow = 0;
    for (const auto& codepoint : neededCodePoints) {
        if (codepoint < 255 && !std::isprint((int)codepoint)) {
            continue;
        }

        auto loadFlags = FT_LOAD_RENDER;
        if (!antialiasing) {
            loadFlags |= FT_LOAD_MONOCHROME;
        }
        if (const auto err = FT_Load_Char(face, codepoint, loadFlags)) {
            std::cout << "Failed to load glyph: " << FT_Error_String(err) << std::endl;
            continue;
        }

        const auto& bmp = face->glyph->bitmap;
        if (pen_x + (int)bmp.width >= aw) {
            pen_x = 0;
            pen_y += maxHeightInRow;
            maxHeightInRow = 0;
        }

        if (antialiasing) {
            for (unsigned int row = 0; row < bmp.rows; ++row) {
                for (unsigned int col = 0; col < bmp.width; ++col) {
                    int x = pen_x + col;
                    int y = pen_y + row;
                    auto value = bmp.buffer[row * bmp.pitch + col];
                    atlasBitmapData[y * aw + x] =
                        Color32{.r = value, .g = value, .b = value, .a = value};
                }
            }
        } else {
            assert(bmp.pixel_mode == FT_PIXEL_MODE_MONO);
            const auto* pixels = bmp.buffer;
            auto* dest = &atlasBitmapData[pen_y * aw + pen_x];
            for (int y = 0; y < (int)bmp.rows; y++) {
                for (int x = 0; x < (int)bmp.width; x++) {
                    std::uint8_t value = ((pixels[x / 8]) & (1 << (7 - (x % 8)))) ? 255 : 0;
                    dest[y * aw + x] = Color32{.r = value, .g = value, .b = value, .a = 255};
                }

                pixels += bmp.pitch;
            }
        }

        auto g = Glyph{
            .uv0 = {pen_x, pen_y},
            .uv1 = {(pen_x + bmp.width), (pen_y + bmp.rows)},
            .bearing = {face->glyph->bitmap_left, face->glyph->bitmap_top},
            .advance = (int)(face->glyph->advance.x >> 6),
        };
        glyphs.emplace(codepoint, std::move(g));

        pen_x += bmp.width;
        if ((int)bmp.rows > maxHeightInRow) {
            maxHeightInRow = (int)bmp.rows;
        }
    }

    loadedCodePoints = neededCodePoints;

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    loaded = true;

    return true;
}

void Font::writeToImage(const std::filesystem::path& path)
{
    static constexpr auto NUM_CHANNELS = 4;
    stbi_write_png(
        path.c_str(),
        atlasSize.x,
        atlasSize.y,
        NUM_CHANNELS,
        atlasBitmapData.data(),
        atlasSize.x * NUM_CHANNELS);
}

glm::vec2 Font::getGlyphAtlasSize() const
{
    return atlasSize;
}

void Font::writeFontInfo(const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);

    static const uint16_t MAGIC = 0x0FAF;
    // magic
    fsutil::binaryWrite(file, MAGIC);

    // main info
    fsutil::binaryWrite(file, (uint8_t)lineSpacing);
    fsutil::binaryWrite(file, (uint8_t)ascenderPx);
    fsutil::binaryWrite(file, (uint8_t)descenderPx);
    fsutil::binaryWrite(file, (uint8_t)0); // pad0
    fsutil::binaryWrite(file, (uint16_t)glyphs.size());

    for (const auto& [c, glyph] : glyphs) {
        // char
        fsutil::binaryWrite(file, (uint16_t)c);
        // uv0
        fsutil::binaryWrite(file, (uint8_t)glyph.uv0.x);
        fsutil::binaryWrite(file, (uint8_t)glyph.uv0.y);
        // size
        fsutil::binaryWrite(file, (uint8_t)(glyph.uv1.x - glyph.uv0.x));
        fsutil::binaryWrite(file, (uint8_t)(glyph.uv1.y - glyph.uv0.y));
        // bearing
        fsutil::binaryWrite(file, (uint8_t)glyph.bearing.x);
        fsutil::binaryWrite(file, (uint8_t)glyph.bearing.y);
        // advance
        fsutil::binaryWrite(file, (uint8_t)glyph.advance);
        // pad
        fsutil::binaryWrite(file, (uint8_t)0);
    }
}
