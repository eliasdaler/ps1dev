#pragma once

#include <cstdint>

// Copied from libgpu.h
//
// CHANGES:
//   using u_char -> std::uint8_t;
//   using u_short -> std::uint16_t;
//   using u_long -> std::uint32_t;

#define setlen(p, _len) (((P_TAG*)(p))->len = (std::uint8_t)(_len))
#define setaddr(p, _addr) (((P_TAG*)(p))->addr = (std::uint32_t)(_addr))
#define setcode(p, _code) (((P_TAG*)(p))->code = (std::uint8_t)(_code))

#define setPolyF3(p) setlen(p, 4), setcode(p, 0x20)
#define setPolyFT3(p) setlen(p, 7), setcode(p, 0x24)
#define setPolyG3(p) setlen(p, 6), setcode(p, 0x30)
#define setPolyGT3(p) setlen(p, 9), setcode(p, 0x34)
#define setPolyF4(p) setlen(p, 5), setcode(p, 0x28)
#define setPolyFT4(p) setlen(p, 9), setcode(p, 0x2c)
#define setPolyG4(p) setlen(p, 8), setcode(p, 0x38)
#define setPolyGT4(p) setlen(p, 12), setcode(p, 0x3c)

#define getTPage(tp, abr, x, y)                                                                 \
    ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3ff) >> 6) | \
     (((y) & 0x200) << 2))

#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))

/*	Primitive 	Lentgh		Code				*/
/*--------------------------------------------------------------------	*/
/*									*/
#define setPolyF3(p) setlen(p, 4), setcode(p, 0x20)
#define setPolyFT3(p) setlen(p, 7), setcode(p, 0x24)
#define setPolyG3(p) setlen(p, 6), setcode(p, 0x30)
#define setPolyGT3(p) setlen(p, 9), setcode(p, 0x34)
#define setPolyF4(p) setlen(p, 5), setcode(p, 0x28)
#define setPolyFT4(p) setlen(p, 9), setcode(p, 0x2c)
#define setPolyG4(p) setlen(p, 8), setcode(p, 0x38)
#define setPolyGT4(p) setlen(p, 12), setcode(p, 0x3c)

#define setSprt8(p) setlen(p, 3), setcode(p, 0x74)
#define setSprt16(p) setlen(p, 3), setcode(p, 0x7c)
#define setSprt(p) setlen(p, 4), setcode(p, 0x64)

#define setTile1(p) setlen(p, 2), setcode(p, 0x68)
#define setTile8(p) setlen(p, 2), setcode(p, 0x70)
#define setTile16(p) setlen(p, 2), setcode(p, 0x78)
#define setTile(p) setlen(p, 3), setcode(p, 0x60)
#define setLineF2(p) setlen(p, 3), setcode(p, 0x40)
#define setLineG2(p) setlen(p, 4), setcode(p, 0x50)
#define setLineF3(p) setlen(p, 5), setcode(p, 0x48), (p)->pad = 0x55555555
#define setLineG3(p) setlen(p, 7), setcode(p, 0x58), (p)->pad = 0x55555555, (p)->p2 = 0
#define setLineF4(p) setlen(p, 6), setcode(p, 0x4c), (p)->pad = 0x55555555
#define setLineG4(p) setlen(p, 9), setcode(p, 0x5c), (p)->pad = 0x55555555, (p)->p2 = 0, (p)->p3 = 0

/*
 * Polygon Primitive Definitions
 */
typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
    std::uint8_t r0, g0, b0, code;
} P_TAG;

typedef struct {
    std::uint8_t r0, g0, b0, code;
} P_CODE;

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
} POLY_F3; /* Flat Triangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    short x3, y3;
} POLY_F4; /* Flat Quadrangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t u0, v0;
    std::uint16_t clut;
    short x1, y1;
    std::uint8_t u1, v1;
    std::uint16_t tpage;
    short x2, y2;
    std::uint8_t u2, v2;
    std::uint16_t pad1;
} POLY_FT3; /* Flat Textured Triangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t u0, v0;
    std::uint16_t clut;
    short x1, y1;
    std::uint8_t u1, v1;
    std::uint16_t tpage;
    short x2, y2;
    std::uint8_t u2, v2;
    std::uint16_t pad1;
    short x3, y3;
    std::uint8_t u3, v3;
    std::uint16_t pad2;
} POLY_FT4; /* Flat Textured Quadrangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t r1, g1, b1, pad1;
    short x1, y1;
    std::uint8_t r2, g2, b2, pad2;
    short x2, y2;
} POLY_G3; /* Gouraud Triangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t r1, g1, b1, pad1;
    short x1, y1;
    std::uint8_t r2, g2, b2, pad2;
    short x2, y2;
    std::uint8_t r3, g3, b3, pad3;
    short x3, y3;
} POLY_G4; /* Gouraud Quadrangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t u0, v0;
    std::uint16_t clut;
    std::uint8_t r1, g1, b1, p1;
    short x1, y1;
    std::uint8_t u1, v1;
    std::uint16_t tpage;
    std::uint8_t r2, g2, b2, p2;
    short x2, y2;
    std::uint8_t u2, v2;
    std::uint16_t pad2;
} POLY_GT3; /* Gouraud Textured Triangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t u0, v0;
    std::uint16_t clut;
    std::uint8_t r1, g1, b1, p1;
    short x1, y1;
    std::uint8_t u1, v1;
    std::uint16_t tpage;
    std::uint8_t r2, g2, b2, p2;
    short x2, y2;
    std::uint8_t u2, v2;
    std::uint16_t pad2;
    std::uint8_t r3, g3, b3, p3;
    short x3, y3;
    std::uint8_t u3, v3;
    std::uint16_t pad3;
} POLY_GT4; /* Gouraud Textured Quadrangle */

/*
 * Line Primitive Definitions
 */
typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
} LINE_F2; /* Unconnected Flat Line */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t r1, g1, b1, p1;
    short x1, y1;
} LINE_G2; /* Unconnected Gouraud Line */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    std::uint32_t pad;
} LINE_F3; /* 2 connected Flat Line */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t r1, g1, b1, p1;
    short x1, y1;
    std::uint8_t r2, g2, b2, p2;
    short x2, y2;
    std::uint32_t pad;
} LINE_G3; /* 2 connected Gouraud Line */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    short x3, y3;
    std::uint32_t pad;
} LINE_F4; /* 3 connected Flat Line Quadrangle */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t r1, g1, b1, p1;
    short x1, y1;
    std::uint8_t r2, g2, b2, p2;
    short x2, y2;
    std::uint8_t r3, g3, b3, p3;
    short x3, y3;
    std::uint32_t pad;
} LINE_G4; /* 3 connected Gouraud Line */

/*
 * Sprite Primitive Definitions
 */
typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t u0, v0;
    std::uint16_t clut;
    short w, h;
} SPRT; /* free size Sprite */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t u0, v0;
    std::uint16_t clut;
} SPRT_16; /* 16x16 Sprite */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    std::uint8_t u0, v0;
    std::uint16_t clut;
} SPRT_8; /* 8x8 Sprite */

/*
 * Tile Primitive Definitions
 */
typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
    short w, h;
} TILE; /* free size Tile */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
} TILE_16; /* 16x16 Tile */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
} TILE_8; /* 8x8 Tile */

typedef struct {
    std::uint32_t tag;
    std::uint8_t r0, g0, b0, code;
    short x0, y0;
} TILE_1; /* 1x1 Tile */

/*
 * Set Primitive Texture Points
 */
#define setUV0(p, _u0, _v0) (p)->u0 = (_u0), (p)->v0 = (_v0)

#define setUV3(p, _u0, _v0, _u1, _v1, _u2, _v2)                                          \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), (p)->u2 = (_u2), \
    (p)->v2 = (_v2)

#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3)                                \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), (p)->u2 = (_u2), \
    (p)->v2 = (_v2), (p)->u3 = (_u3), (p)->v3 = (_v3)

#define setUVWH(p, _u0, _v0, _w, _h)                                                            \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u0) + (_w), (p)->v1 = (_v0), (p)->u2 = (_u0), \
    (p)->v2 = (_v0) + (_h), (p)->u3 = (_u0) + (_w), (p)->v3 = (_v0) + (_h)

/*
 * Set Primitive Colors
 */
#define setRGB0(p, _r0, _g0, _b0) (p)->r0 = _r0, (p)->g0 = _g0, (p)->b0 = _b0

#define setRGB1(p, _r1, _g1, _b1) (p)->r1 = _r1, (p)->g1 = _g1, (p)->b1 = _b1

#define setRGB2(p, _r2, _g2, _b2) (p)->r2 = _r2, (p)->g2 = _g2, (p)->b2 = _b2

#define setRGB3(p, _r3, _g3, _b3) (p)->r3 = _r3, (p)->g3 = _g3, (p)->b3 = _b3
