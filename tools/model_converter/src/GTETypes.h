#pragma once

#include <cstdint>

#define setlen(p, _len) (((P_TAG*)(p))->len = (u_char)(_len))
#define setaddr(p, _addr) (((P_TAG*)(p))->addr = (u_long)(_addr))
#define setcode(p, _code) (((P_TAG*)(p))->code = (u_char)(_code))

#define setPolyF3(p) setlen(p, 4), setcode(p, 0x20)
#define setPolyFT3(p) setlen(p, 7), setcode(p, 0x24)
#define setPolyG3(p) setlen(p, 6), setcode(p, 0x30)
#define setPolyGT3(p) setlen(p, 9), setcode(p, 0x34)
#define setPolyF4(p) setlen(p, 5), setcode(p, 0x28)
#define setPolyFT4(p) setlen(p, 9), setcode(p, 0x2c)
#define setPolyG4(p) setlen(p, 8), setcode(p, 0x38)
#define setPolyGT4(p) setlen(p, 12), setcode(p, 0x3c)

#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))

#define getTPage(tp, abr, x, y)                                                                 \
    ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3ff) >> 6) | \
     (((y) & 0x200) << 2))

struct P_TAG {
    unsigned addr : 24;
    unsigned len : 8;
    std::uint8_t r0, g0, b0, code;
};

struct GouraudTexturedQuad {
    std::uint32_t tag;

    std::uint8_t r0, g0, b0, code;
    std::int16_t x0, y0;

    std::uint8_t u0, v0;
    std::uint16_t clut;

    std::uint8_t r1, g1, b1, p1;
    std::int16_t x1, y1;
    std::uint8_t u1, v1;
    std::uint16_t tpage;

    std::uint8_t r2, g2, b2, p2;
    std::int16_t x2, y2;
    std::uint8_t u2, v2;
    std::uint16_t pad2;

    std::uint8_t r3, g3, b3, p3;
    std::int16_t x3, y3;
    std::uint8_t u3, v3;
    std::uint16_t pad3;
};
