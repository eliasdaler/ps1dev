struct Vert {
    s16 x, y, z, w;
    u8 r,g,b,pad;
};

struct POLY_GT3 {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 u1, v1;
    s16 tpage;
    u8 r2, g2, b2, p2;
    s16  x2, y2;
    u8 u2, v2;
    u16 pad2;
}; /* Gouraud Textured Triangle */

struct POLY_GT4 {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, p2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad2;
    u8 r3, g3, b3, p3;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad3;
}; /* Gouraud Textured Quadrangle */

struct FastModel {
    u16 numVerts;
    Vert verts[numVerts];
    u16 numTris;
    u16 numQuads;
    POLY_GT3 polygt3s[numTris];
    POLY_GT4 polygt4s[numQuads];
};

FastModel fm @ 0x0;