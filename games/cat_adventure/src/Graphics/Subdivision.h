#pragma once

#include <cstdint>

#include "Model.h"
#include "Renderer.h"

#define SCRATCH_PAD 0x1f800000

// #define FULL_INLINE

struct UVCoords {
    std::uint8_t u, v, pad1, pad2;
};

// data used when subdividing quad into 4 quads
struct SubdivData1 {
    // original vertex attributes
    Vec3Pad ov[4];
    UVCoords ouv[4];
    psyqo::Color ocol[4];

    // attributes of current subdivided quad
    Vec3Pad v[4];
    UVCoords uv[4];
    psyqo::Color col[4];
};

// data used when subdividing quad into 16 quads
struct SubdivData2 : SubdivData1 {
    // original vertex attributes
    Vec3Pad oov[4];
    UVCoords oouv[4];
    psyqo::Color oocol[4];
};

inline constexpr int LEVEL_1_SUBDIV_DIST = 3000;
inline constexpr int LEVEL_2_SUBDIV_DIST = 1500;

// Interpolate from ov_i to ov_j, store into v_d
#define INTERP_ATTR(wrk, d, i, j)                                                     \
    wrk.v[(d)].pos.x.value = (wrk.ov[(i)].pos.x.value + wrk.ov[(j)].pos.x.value) / 2; \
    wrk.v[(d)].pos.y.value = (wrk.ov[(i)].pos.y.value + wrk.ov[(j)].pos.y.value) / 2; \
    wrk.v[(d)].pos.z.value = (wrk.ov[(i)].pos.z.value + wrk.ov[(j)].pos.z.value) / 2; \
    wrk.uv[(d)].u = (wrk.ouv[(i)].u + wrk.ouv[(j)].u) / 2;                            \
    wrk.uv[(d)].v = (wrk.ouv[(i)].v + wrk.ouv[(j)].v) / 2;                            \
    wrk.col[(d)].r = (wrk.ocol[(i)].r + wrk.ocol[(j)].r) / 2;                         \
    wrk.col[(d)].g = (wrk.ocol[(i)].g + wrk.ocol[(j)].g) / 2;                         \
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
#define INTERP_ATTR2(wrk, d, i, j)                                                       \
    wrk.ov[(d)].pos.x.value = (wrk.oov[(i)].pos.x.value + wrk.oov[(j)].pos.x.value) / 2; \
    wrk.ov[(d)].pos.y.value = (wrk.oov[(i)].pos.y.value + wrk.oov[(j)].pos.y.value) / 2; \
    wrk.ov[(d)].pos.z.value = (wrk.oov[(i)].pos.z.value + wrk.oov[(j)].pos.z.value) / 2; \
    wrk.ouv[(d)].u = (wrk.oouv[(i)].u + wrk.oouv[(j)].u) / 2;                            \
    wrk.ouv[(d)].v = (wrk.oouv[(i)].v + wrk.oouv[(j)].v) / 2;                            \
    wrk.ocol[(d)].r = (wrk.oocol[(i)].r + wrk.oocol[(j)].r) / 2;                         \
    wrk.ocol[(d)].g = (wrk.oocol[(i)].g + wrk.oocol[(j)].g) / 2;                         \
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

#ifdef FULL_INLINE
#define DRAW_QUAD(wrk)                                                                          \
    {                                                                                           \
        auto& quadFrag =                                                                        \
            primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>(prim, wrk.col[0]);    \
        auto& quad2d = quadFrag.primitive;                                                      \
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(wrk.v[0].pos);                  \
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(wrk.v[1].pos);                  \
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(wrk.v[2].pos);                    \
        psyqo::GTE::Kernels::rtpt();                                                            \
        psyqo::GTE::Kernels::nclip();                                                           \
        const auto dot =                                                                        \
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();       \
        if (dot > 0) {                                                                          \
            psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);                \
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(wrk.v[3].pos);                \
            psyqo::GTE::Kernels::rtps();                                                        \
            psyqo::GTE::Kernels::avsz4();                                                       \
            auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();     \
            avgZ += bias + addBias; /* add bias */                                              \
            if (avgZ > 0 && avgZ < Renderer::OT_SIZE) {                                         \
                /* quad2d.command = (command & 0xFF000000) | (wrk.col[0].packed & 0xffffff); */ \
                quad2d.colorB = wrk.col[1];                                                     \
                quad2d.colorC = wrk.col[2];                                                     \
                quad2d.colorD = wrk.col[3];                                                     \
                quad2d.tpage = tpage;                                                           \
                quad2d.clutIndex = clut;                                                        \
                quad2d.uvA.u = wrk.uv[0].u;                                                     \
                quad2d.uvA.v = wrk.uv[0].v;                                                     \
                quad2d.uvB.u = wrk.uv[1].u;                                                     \
                quad2d.uvB.v = wrk.uv[1].v;                                                     \
                quad2d.uvC.u = wrk.uv[2].u;                                                     \
                quad2d.uvC.v = wrk.uv[2].v;                                                     \
                quad2d.uvD.u = wrk.uv[3].u;                                                     \
                quad2d.uvD.v = wrk.uv[3].v;                                                     \
                psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);            \
                psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);            \
                psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);            \
                ot.insert(quadFrag, avgZ);                                                      \
            }                                                                                   \
        }                                                                                       \
    }
#else
#define DRAW_QUAD(wrk) drawQuadImpl(wrk, prim, ot, primBuffer, bias + addBias, tpage, clut)

template<typename T>
void drawQuadImpl(
    T& wrk,
    const psyqo::Prim::GouraudTexturedQuad& prim,
    Renderer::OrderingTableType& ot,
    Renderer::PrimBufferAllocatorType& primBuffer,
    int bias,
    psyqo::PrimPieces::TPageAttr tpage,
    psyqo::PrimPieces::ClutIndex clut)
{
    auto& quadFrag =
        primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>(prim, wrk.col[0]);
    auto& quad2d = quadFrag.primitive;
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(wrk.v[0].pos);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(wrk.v[1].pos);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(wrk.v[2].pos);
    psyqo::GTE::Kernels::rtpt();
    psyqo::GTE::Kernels::nclip();
    const auto dot = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
    if (dot > 0) {
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(wrk.v[3].pos);
        psyqo::GTE::Kernels::rtps();
        psyqo::GTE::Kernels::avsz4();
        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        avgZ += bias; /* add bias */
        if (avgZ > 0 && avgZ < Renderer::OT_SIZE) {
            /* quad2d.command = (command & 0xFF000000) | (wrk.col[0].packed & 0xffffff); */
            quad2d.colorB = wrk.col[1];
            quad2d.colorC = wrk.col[2];
            quad2d.colorD = wrk.col[3];
            quad2d.tpage = tpage;
            quad2d.clutIndex = clut;
            quad2d.uvA.u = wrk.uv[0].u;
            quad2d.uvA.v = wrk.uv[0].v;
            quad2d.uvB.u = wrk.uv[1].u;
            quad2d.uvB.v = wrk.uv[1].v;
            quad2d.uvC.u = wrk.uv[2].u;
            quad2d.uvC.v = wrk.uv[2].v;
            quad2d.uvD.u = wrk.uv[3].u;
            quad2d.uvD.v = wrk.uv[3].v;
            psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);
            ot.insert(quadFrag, avgZ);
        }
    }
}
#endif

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
