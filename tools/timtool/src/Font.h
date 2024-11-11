#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <glm/vec2.hpp>

#include "Color.h"

struct Glyph {
    glm::ivec2 uv0; // top left (in pixels)
    glm::ivec2 uv1; // bottom right (in pixels)
    glm::ivec2 bearing; // top left corner, relative to origin on the baseline
    int advance{0}; // offset to the next char in pixels
};

struct Font {
    bool load(const std::filesystem::path& path, int size, bool antialiasing = true);
    bool load(
        const std::filesystem::path& path,
        int size,
        const std::unordered_set<std::uint32_t>& neededCodePoints,
        bool antialiasing = true);

    void writeToImage(const std::filesystem::path& path);
    void writeFontInfo(const std::filesystem::path& path);

    glm::vec2 getGlyphAtlasSize() const;

    void forEachGlyph(
        const std::string& text,
        std::function<void(const glm::vec2& pos, const glm::vec2& uv0, const glm::vec2& uv1)> f)
        const;

    // data
    std::unordered_map<std::uint32_t, Glyph> glyphs;
    std::unordered_set<std::uint32_t> loadedCodePoints;

    // grayscale bitmap data
    std::vector<Color32> atlasBitmapData;
    glm::vec2 atlasSize;

    int size{0};
    float lineSpacing{0}; // line spacing in pixels
    float ascenderPx{0.f};
    float descenderPx{0.f};
    bool loaded{false};
    bool antialias{false};
};
