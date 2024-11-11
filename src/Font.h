#pragma once

#include <cstdint>

#include <EASTL/array.h>
#include <EASTL/vector.h>

// TODO: replace with Vector<2, 0> later
template<typename T>
struct IVec2 {
    T x, y;
};

struct Glyph {
    IVec2<std::uint8_t> uv0; // top left (in pixels)
    IVec2<std::uint8_t> size; // glyph size
    IVec2<std::uint8_t> bearing; // top left corner, relative to origin on the baseline
    std::uint8_t advance{0}; // offset to the next char in pixels
    std::uint8_t pad{0};
};

class Font {
public:
    void loadFromFile(const eastl::vector<uint8_t>& data);

    uint8_t lineSpacing;
    uint8_t ascenderPx;
    uint8_t descenderPx;
    eastl::array<Glyph, 255> glyphs; // TODO: support non-ASCII
};
