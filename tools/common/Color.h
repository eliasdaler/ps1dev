#pragma once

#include <cstdint>
#include <format>
#include <iostream>

struct Color32 {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;

    auto operator<=>(const Color32&) const = default;
};

using Color16 = std::uint16_t;

inline std::uint8_t from8BitTo5Bit(std::uint8_t v)
{
    // A hacky way: ((v * 249) + 1014) >> 11
    return ((v * 31) + 127) / 255;
}

inline std::uint8_t from5BitTo8Bit(std::uint8_t v)
{
    return static_cast<std::uint8_t>(((float)v / 31.f) * 255.f);
}

inline Color32 from16bitColor(Color16 c)
{
    return Color32{
        .r = from5BitTo8Bit(static_cast<std::uint8_t>(c & 0b0000000000011111)),
        .g = from5BitTo8Bit(static_cast<std::uint8_t>((c & 0b0000001111100000) >> 5)),
        .b = from5BitTo8Bit(static_cast<std::uint8_t>((c & 0b0111110000000000) >> 10)),
        .a = static_cast<std::uint8_t>(((c & 0b1000000000000000) >> 15 == 0) ? 255 : 0),
    };
}

inline void printColor(const Color32& c)
{
    std::cout << std::format("{{ {}, {}, {}, {} }}\n", c.r, c.g, c.b, c.a);
}

inline void printColorQuant(const Color32& c)
{
    std::cout << std::format(
        "{{ {}, {}, {} }}\n", from8BitTo5Bit(c.r), from8BitTo5Bit(c.g), from8BitTo5Bit(c.b));
}

inline std::uint32_t toU32Color(const Color32& c)
{
    return ((c.a << 24) | (c.r << 16) | (c.g << 8) | c.b);
}
