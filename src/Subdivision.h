#pragma once

#include "libgpu.h"

// data used when subdividing quad into 4 quads
struct SubdivData1 {
    // original vertex attributes
    SVECTOR ov[4];
    CVECTOR ouv[4];
    CVECTOR ocol[4];

    // attributes of current subdivided quad
    SVECTOR v[4];
    CVECTOR uv[4];
    CVECTOR col[4];

    CVECTOR intCol;
};

// data used when subdividing quad into 16 quads
struct SubdivData2 {
    // attributes for 2x2 subdivided quad
    SVECTOR ov[4];
    CVECTOR ouv[4];
    CVECTOR ocol[4];

    // attributes of current subdivided quad
    SVECTOR v[4];
    CVECTOR uv[4];
    CVECTOR col[4];

    CVECTOR intCol;

    // original vertex attributes
    SVECTOR oov[4];
    CVECTOR oouv[4];
    CVECTOR oocol[4];
};

// check that it fits into scratch pad
static_assert(sizeof(SubdivData1) < 1024);
static_assert(sizeof(SubdivData2) < 1024);

// Interpolate from ov_i to ov_j, store into v_d
#define INTERP_ATTR(wrk, d, i, j)                             \
    wrk.v[(d)].vx = (wrk.ov[(i)].vx + wrk.ov[(j)].vx) / 2;    \
    wrk.v[(d)].vy = (wrk.ov[(i)].vy + wrk.ov[(j)].vy) / 2;    \
    wrk.v[(d)].vz = (wrk.ov[(i)].vz + wrk.ov[(j)].vz) / 2;    \
    wrk.uv[(d)].r = (wrk.ouv[(i)].r + wrk.ouv[(j)].r) / 2;    \
    wrk.uv[(d)].g = (wrk.ouv[(i)].g + wrk.ouv[(j)].g) / 2;    \
    wrk.col[(d)].r = (wrk.ocol[(i)].r + wrk.ocol[(j)].r) / 2; \
    wrk.col[(d)].g = (wrk.ocol[(i)].g + wrk.ocol[(j)].g) / 2; \
    wrk.col[(d)].b = (wrk.ocol[(i)].b + wrk.ocol[(j)].b) / 2;

// Copy from ov_i to v_d
#define COPY_ATTR(wrk, d, i)    \
    wrk.v[(d)] = wrk.ov[(i)];   \
    wrk.uv[(d)] = wrk.ouv[(i)]; \
    wrk.col[(d)] = wrk.ocol[(i)];

// Copy from v_i to v_d
#define COPY_CALC_ATTR(wrk, d, i) \
    wrk.v[(d)] = wrk.v[(i)];      \
    wrk.uv[(d)] = wrk.uv[(i)];    \
    wrk.col[(d)] = wrk.col[(i)];

// Interpolate from oov_i to oov_j, store into ov_d
#define INTERP_ATTR2(wrk, d, i, j)                               \
    wrk.ov[(d)].vx = (wrk.oov[(i)].vx + wrk.oov[(j)].vx) / 2;    \
    wrk.ov[(d)].vy = (wrk.oov[(i)].vy + wrk.oov[(j)].vy) / 2;    \
    wrk.ov[(d)].vz = (wrk.oov[(i)].vz + wrk.oov[(j)].vz) / 2;    \
    wrk.ouv[(d)].r = (wrk.oouv[(i)].r + wrk.oouv[(j)].r) / 2;    \
    wrk.ouv[(d)].g = (wrk.oouv[(i)].g + wrk.oouv[(j)].g) / 2;    \
    wrk.ocol[(d)].r = (wrk.oocol[(i)].r + wrk.oocol[(j)].r) / 2; \
    wrk.ocol[(d)].g = (wrk.oocol[(i)].g + wrk.oocol[(j)].g) / 2; \
    wrk.ocol[(d)].b = (wrk.oocol[(i)].b + wrk.oocol[(j)].b) / 2;

// Copy from oov_i to ov_d
#define COPY_ATTR2(wrk, d, i)     \
    wrk.ov[(d)] = wrk.oov[(i)];   \
    wrk.ouv[(d)] = wrk.oouv[(i)]; \
    wrk.ocol[(d)] = wrk.oocol[(i)];

// Copy from ov_i to ov_d
#define COPY_CALC_ATTR2(wrk, d, i) \
    wrk.ov[(d)] = wrk.ov[(i)];     \
    wrk.ouv[(d)] = wrk.ouv[(i)];   \
    wrk.ocol[(d)] = wrk.ocol[(i)];

#define INTERP_COLOR_GTE_DIV(wrk, i)           \
    gte_DpqColor(&wrk.col[i], p, &wrk.intCol); \
    setRGB##i(polygt4, wrk.intCol.r, wrk.intCol.g, wrk.intCol.b);

// Draw 2x2 quads
#define DRAW_QUAD(wrk)                                        \
    polygt4 = (POLY_GT4*)nextpri;                             \
    setPolyGT4(polygt4);                                      \
    polygt4->tpage = tpage;                                   \
    polygt4->clut = clut;                                     \
                                                              \
    setUV4(                                                   \
        polygt4,                                              \
        wrk.uv[0].r,                                          \
        wrk.uv[0].g,                                          \
        wrk.uv[1].r,                                          \
        wrk.uv[1].g,                                          \
        wrk.uv[2].r,                                          \
        wrk.uv[2].g,                                          \
        wrk.uv[3].r,                                          \
        wrk.uv[3].g);                                         \
                                                              \
    gte_ldv0(&wrk.v[0]);                                      \
    gte_ldv1(&wrk.v[1]);                                      \
    gte_ldv2(&wrk.v[2]);                                      \
                                                              \
    gte_rtpt();                                               \
                                                              \
    gte_stsxy0(&polygt4->x0);                                 \
                                                              \
    gte_ldv0(&wrk.v[3]);                                      \
    gte_rtps();                                               \
                                                              \
    gte_avsz4();                                              \
    gte_stotz(&otz);                                          \
    /* unfortunately, this introduces some branching */       \
    /* maybe we can avoid it somehow? */                      \
    if (otz > 0 && otz < OTLEN) {                             \
        gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3); \
        gte_stdp(&p);                                         \
                                                              \
        INTERP_COLOR_GTE_DIV(wrk, 0);                         \
        INTERP_COLOR_GTE_DIV(wrk, 1);                         \
        INTERP_COLOR_GTE_DIV(wrk, 2);                         \
        INTERP_COLOR_GTE_DIV(wrk, 3);                         \
                                                              \
        addPrim(&ot[otz], polygt4);                           \
        nextpri += sizeof(POLY_GT4);                          \
    }

// Draws 4 quads (out of 4) for 4x4 subdiv
#define DRAW_QUADS_22(wrk)     \
    COPY_ATTR(wrk, 0, 0);      \
    INTERP_ATTR(wrk, 1, 0, 1); \
    INTERP_ATTR(wrk, 2, 0, 2); \
    INTERP_ATTR(wrk, 3, 0, 3); \
    DRAW_QUAD(wrk);            \
                               \
    COPY_CALC_ATTR(wrk, 0, 1); \
    COPY_CALC_ATTR(wrk, 2, 3); \
    COPY_ATTR(wrk, 1, 1);      \
    INTERP_ATTR(wrk, 3, 1, 3); \
    DRAW_QUAD(wrk);            \
                               \
    COPY_CALC_ATTR(wrk, 0, 2); \
    COPY_CALC_ATTR(wrk, 1, 3); \
    INTERP_ATTR(wrk, 2, 2, 3); \
    COPY_ATTR(wrk, 3, 3);      \
    DRAW_QUAD(wrk);            \
                               \
    COPY_CALC_ATTR(wrk, 1, 0); \
    COPY_CALC_ATTR(wrk, 3, 2); \
    INTERP_ATTR(wrk, 0, 0, 2); \
    COPY_ATTR(wrk, 2, 2);      \
    DRAW_QUAD(wrk);

// draws 4x4 quads
#define DRAW_QUADS_44(wrk)      \
    COPY_ATTR2(wrk, 0, 0);      \
    INTERP_ATTR2(wrk, 1, 0, 1); \
    INTERP_ATTR2(wrk, 2, 0, 2); \
    INTERP_ATTR2(wrk, 3, 0, 3); \
    DRAW_QUADS_22(wrk);         \
                                \
    COPY_CALC_ATTR2(wrk, 0, 1); \
    COPY_ATTR2(wrk, 1, 1);      \
    COPY_CALC_ATTR2(wrk, 2, 3); \
    INTERP_ATTR2(wrk, 3, 1, 3); \
    DRAW_QUADS_22(wrk);         \
                                \
    COPY_CALC_ATTR2(wrk, 0, 2); \
    COPY_CALC_ATTR2(wrk, 1, 3); \
    INTERP_ATTR2(wrk, 2, 2, 3); \
    COPY_ATTR2(wrk, 3, 3);      \
    DRAW_QUADS_22(wrk);         \
                                \
    COPY_CALC_ATTR2(wrk, 1, 0); \
    COPY_CALC_ATTR2(wrk, 3, 2); \
    COPY_ATTR2(wrk, 2, 2);      \
    INTERP_ATTR2(wrk, 0, 0, 2); \
    DRAW_QUADS_22(wrk);
