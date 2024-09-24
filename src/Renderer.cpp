#include "Renderer.h"

#include "Clip.h"
#include "FastModel.h"
#include "Model.h"
#include "Object.h"
#include "Camera.h"
#include "Globals.h"

#include <inline_n.h>
#include <gtemac.h>

namespace
{
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

}

void Renderer::drawModel(Object& object, const Model& model, TIM_IMAGE& texture, bool subdivide)
{
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    for (const auto& mesh : model.meshes) {
        drawMesh(object, mesh, texture, subdivide);
    }
}

void Renderer::drawMesh(Object& object, const Mesh& mesh, const TIM_IMAGE& texture, bool subdivide)
{
    const auto tpage = getTPage(texture.mode & 0x3, 0, texture.prect->x, texture.prect->y);
    const auto clut = getClut(texture.crect->x, texture.crect->y);

    std::size_t vertexIdx = 0;

    // TODO: subdiv for triangles
    for (std::size_t i = 0; i < mesh.numTris; ++i, vertexIdx += 3) {
        auto* polyft3 = (POLY_FT3*)nextpri;
        setPolyFT3(polyft3);

        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];

        setUV3(polyft3, v0.uv.r, v0.uv.g, v1.uv.r, v1.uv.g, v2.uv.r, v2.uv.g);

        polyft3->tpage = tpage;
        polyft3->clut = clut;

        gte_ldv0(&v0.pos);
        gte_ldv1(&v1.pos);
        gte_ldv2(&v2.pos);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        gte_stsxy3(&polyft3->x0, &polyft3->x1, &polyft3->x2);

        gte_avsz3();

        long otz;
        gte_stotz(&otz);

        long p;
        gte_stdp(&p);

        CVECTOR near = {128, 128, 128, 44};
        CVECTOR col;
        gte_DpqColor(&near, p, &col);
        setRGB0(polyft3, col.r, col.g, col.b);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            CVECTOR col;

            addPrim(&ot[otz], polyft3);
            nextpri += sizeof(POLY_FT3);
        }
    }

    if (!subdivide) {
        drawQuads(mesh, tpage, clut);
        return;
    }

    auto& wrk1 = *(SubdivData1*)(SCRATCH_PAD + 0xC0);

    for (int i = 0; i < mesh.numQuads; ++i, vertexIdx += 4) {
        auto* polygt4 = (POLY_GT4*)nextpri;
        setPolyGT4(polygt4);

        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];

        gte_ldv0(&v0.pos);
        gte_ldv1(&v1.pos);
        gte_ldv2(&v2.pos);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        gte_stsxy0(&polygt4->x0);

        const auto& v3 = mesh.vertices[vertexIdx + 3];

        gte_ldv0(&v3.pos);
        gte_rtps();

        gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3);

        if (quad_clip(
                &screenClip,
                (DVECTOR*)&polygt4->x0,
                (DVECTOR*)&polygt4->x1,
                (DVECTOR*)&polygt4->x2,
                (DVECTOR*)&polygt4->x3)) {
            continue;
        }

        wrk1.ov[0] = v0.pos;
        wrk1.ov[1] = v1.pos;
        wrk1.ov[2] = v2.pos;
        wrk1.ov[3] = v3.pos;

        wrk1.ouv[0] = v0.uv;
        wrk1.ouv[1] = v1.uv;
        wrk1.ouv[2] = v2.uv;
        wrk1.ouv[3] = v3.uv;

        wrk1.ocol[0] = v0.col;
        wrk1.ocol[1] = v1.col;
        wrk1.ocol[2] = v2.col;
        wrk1.ocol[3] = v3.col;

#define INTERP_ATTR(wrk, d, i, j)                             \
    wrk.v[(d)].vx = (wrk.ov[(i)].vx + wrk.ov[(j)].vx) / 2;    \
    wrk.v[(d)].vy = (wrk.ov[(i)].vy + wrk.ov[(j)].vy) / 2;    \
    wrk.v[(d)].vz = (wrk.ov[(i)].vz + wrk.ov[(j)].vz) / 2;    \
    wrk.uv[(d)].r = (wrk.ouv[(i)].r + wrk.ouv[(j)].r) / 2;    \
    wrk.uv[(d)].g = (wrk.ouv[(i)].g + wrk.ouv[(j)].g) / 2;    \
    wrk.col[(d)].r = (wrk.ocol[(i)].r + wrk.ocol[(j)].r) / 2; \
    wrk.col[(d)].g = (wrk.ocol[(i)].g + wrk.ocol[(j)].g) / 2; \
    wrk.col[(d)].b = (wrk.ocol[(i)].b + wrk.ocol[(j)].b) / 2;

#define COPY_ATTR(wrk, d, i)    \
    wrk.v[(d)] = wrk.ov[(i)];   \
    wrk.uv[(d)] = wrk.ouv[(i)]; \
    wrk.col[(d)] = wrk.ocol[(i)];

#define COPY_CALC_ATTR(wrk, d, i) \
    wrk.v[(d)] = wrk.v[(i)];      \
    wrk.uv[(d)] = wrk.uv[(i)];    \
    wrk.col[(d)] = wrk.col[(i)];

#define INTERP_ATTR2(wrk, d, i, j)                               \
    wrk.ov[(d)].vx = (wrk.oov[(i)].vx + wrk.oov[(j)].vx) / 2;    \
    wrk.ov[(d)].vy = (wrk.oov[(i)].vy + wrk.oov[(j)].vy) / 2;    \
    wrk.ov[(d)].vz = (wrk.oov[(i)].vz + wrk.oov[(j)].vz) / 2;    \
    wrk.ouv[(d)].r = (wrk.oouv[(i)].r + wrk.oouv[(j)].r) / 2;    \
    wrk.ouv[(d)].g = (wrk.oouv[(i)].g + wrk.oouv[(j)].g) / 2;    \
    wrk.ocol[(d)].r = (wrk.oocol[(i)].r + wrk.oocol[(j)].r) / 2; \
    wrk.ocol[(d)].g = (wrk.oocol[(i)].g + wrk.oocol[(j)].g) / 2; \
    wrk.ocol[(d)].b = (wrk.oocol[(i)].b + wrk.oocol[(j)].b) / 2;

#define COPY_ATTR2(wrk, d, i)     \
    wrk.ov[(d)] = wrk.oov[(i)];   \
    wrk.ouv[(d)] = wrk.oouv[(i)]; \
    wrk.ocol[(d)] = wrk.oocol[(i)];

#define COPY_CALC_ATTR2(wrk, d, i) \
    wrk.ov[(d)] = wrk.ov[(i)];     \
    wrk.ouv[(d)] = wrk.ouv[(i)];   \
    wrk.ocol[(d)] = wrk.ocol[(i)];

#define INTERP_COLOR_GTE(wrk, i)               \
    gte_DpqColor(&wrk.col[i], p, &wrk.intCol); \
    setRGB##i(polygt4, wrk.intCol.r, wrk.intCol.g, wrk.intCol.b);

#define DRAW_QUAD(wrk)                                    \
    polygt4 = (POLY_GT4*)nextpri;                         \
    setPolyGT4(polygt4);                                  \
    polygt4->tpage = tpage;                               \
    polygt4->clut = clut;                                 \
                                                          \
    setUV4(                                               \
        polygt4,                                          \
        wrk.uv[0].r,                                      \
        wrk.uv[0].g,                                      \
        wrk.uv[1].r,                                      \
        wrk.uv[1].g,                                      \
        wrk.uv[2].r,                                      \
        wrk.uv[2].g,                                      \
        wrk.uv[3].r,                                      \
        wrk.uv[3].g);                                     \
                                                          \
    gte_ldv0(&wrk.v[0]);                                  \
    gte_ldv1(&wrk.v[1]);                                  \
    gte_ldv2(&wrk.v[2]);                                  \
                                                          \
    gte_rtpt();                                           \
                                                          \
    gte_stsxy0(&polygt4->x0);                             \
                                                          \
    gte_ldv0(&wrk.v[3]);                                  \
    gte_rtps();                                           \
                                                          \
    gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3); \
    gte_stdp(&p);                                         \
                                                          \
    INTERP_COLOR_GTE(wrk, 0);                             \
    INTERP_COLOR_GTE(wrk, 1);                             \
    INTERP_COLOR_GTE(wrk, 2);                             \
    INTERP_COLOR_GTE(wrk, 3);                             \
                                                          \
    addPrim(&ot[otz], polygt4);                           \
    nextpri += sizeof(POLY_GT4);

#define DRAW_SUB_QUAD(wrk)     \
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

        long otz;
        gte_avsz4();
        gte_stotz(&otz);

        if (otz < 200) {
            // subdiv level 2
            long p;

            auto& wrk2 = *reinterpret_cast<SubdivData2*>(&wrk1);

            for (int i = 0; i < 4; ++i) {
                wrk2.oov[i] = wrk2.ov[i];
                wrk2.oouv[i] = wrk2.ouv[i];
                wrk2.oocol[i] = wrk2.ocol[i];
            }

            // QUAD 0
            COPY_ATTR2(wrk2, 0, 0);
            INTERP_ATTR2(wrk2, 1, 0, 1);
            INTERP_ATTR2(wrk2, 2, 0, 2);
            INTERP_ATTR2(wrk2, 3, 0, 3);

            DRAW_SUB_QUAD(wrk2);

            // QUAD 1
            COPY_CALC_ATTR2(wrk2, 0, 1);
            COPY_ATTR2(wrk2, 1, 1);
            COPY_CALC_ATTR2(wrk2, 2, 3);
            INTERP_ATTR2(wrk2, 3, 1, 3);

            DRAW_SUB_QUAD(wrk2);

            // QUAD 2
            COPY_CALC_ATTR2(wrk2, 0, 2);
            COPY_CALC_ATTR2(wrk2, 1, 3);
            INTERP_ATTR2(wrk2, 2, 2, 3);
            COPY_ATTR2(wrk2, 3, 3);

            DRAW_SUB_QUAD(wrk2);

            // QUAD 3
            COPY_CALC_ATTR2(wrk2, 1, 0);
            COPY_CALC_ATTR2(wrk2, 3, 2);
            COPY_ATTR2(wrk2, 2, 2);
            INTERP_ATTR2(wrk2, 0, 0, 2);

            DRAW_SUB_QUAD(wrk2);

            continue;
        }

        if (otz < 700) {
            // subdiv level 1
            long p;
            DRAW_SUB_QUAD(wrk1);
            continue;
        }

        long p;
        gte_stdp(&p);

#define INTERP_COLOR_GTE2(wrk, i)          \
    gte_DpqColor(&wrk.ocol[(i)], p, &col); \
    setRGB##i(polygt4, col.r, col.g, col.b);

        CVECTOR near, col;
        INTERP_COLOR_GTE2(wrk1, 0);
        INTERP_COLOR_GTE2(wrk1, 1);
        INTERP_COLOR_GTE2(wrk1, 2);
        INTERP_COLOR_GTE2(wrk1, 3);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            polygt4->tpage = tpage;
            polygt4->clut = clut;
            setUV4(
                polygt4,
                wrk1.ouv[0].r,
                wrk1.ouv[0].g,
                wrk1.ouv[1].r,
                wrk1.ouv[1].g,
                wrk1.ouv[2].r,
                wrk1.ouv[2].g,
                wrk1.ouv[3].r,
                wrk1.ouv[3].g);

            addPrim(&ot[otz], polygt4);
            nextpri += sizeof(POLY_GT4);
        }
    }
}

void Renderer::drawQuads(const Mesh& mesh, u_long tpage, int clut)
{
    auto vertexIdx = mesh.numTris * 3;
    for (int i = 0; i < mesh.numQuads; ++i, vertexIdx += 4) {
        auto* polygt4 = (POLY_GT4*)nextpri;
        setPolyGT4(polygt4);

        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];
        const auto& v3 = mesh.vertices[vertexIdx + 3];

        setUV4(polygt4, v0.uv.r, v0.uv.g, v1.uv.r, v1.uv.g, v2.uv.r, v2.uv.g, v3.uv.r, v3.uv.g);

        polygt4->tpage = tpage;
        polygt4->clut = clut;

        gte_ldv0(&v0);
        gte_ldv1(&v1);
        gte_ldv2(&v2);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        gte_stsxy0(&polygt4->x0);

        gte_ldv0(&v3);
        gte_rtps();

        gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3);

        /* if (quad_clip(
                &screenClip,
                (DVECTOR*)&polyft4->x0,
                (DVECTOR*)&polyft4->x1,
                (DVECTOR*)&polyft4->x2,
                (DVECTOR*)&polyft4->x3)) {
            return;
        } */

        long otz;
        gte_avsz4();
        gte_stotz(&otz);

        long p;
        gte_stdp(&p);

#define INTERP_COLOR_GTE3(i)          \
    gte_DpqColor(&v##i.col, p, &col); \
    setRGB##i(polygt4, col.r, col.g, col.b);

        CVECTOR col, near;
        INTERP_COLOR_GTE3(0);
        INTERP_COLOR_GTE3(1);
        INTERP_COLOR_GTE3(2);
        INTERP_COLOR_GTE3(3);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            CVECTOR col;

            addPrim(&ot[otz], polygt4);
            nextpri += sizeof(POLY_GT4);
        }
    }
}

void Renderer::drawModelFast(Object& object, const FastModelInstance& fm)
{
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    auto& trianglePrims = fm.trianglePrims[currBuffer];
    const auto numTris = trianglePrims.size();
    for (std::uint16_t triIndex = 0; triIndex < numTris; ++triIndex) {
        const auto& v0 = fm.vertices[triIndex * 3 + 0];
        const auto& v1 = fm.vertices[triIndex * 3 + 1];
        const auto& v2 = fm.vertices[triIndex * 3 + 2];

        gte_ldv0(&v0);
        gte_ldv1(&v1);
        gte_ldv2(&v2);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        auto* polygt3 = &trianglePrims[triIndex];
        gte_stsxy3(&polygt3->x0, &polygt3->x1, &polygt3->x2);

        gte_avsz3();
        long otz;
        gte_stotz(&otz);

        otz -= 64; // depth bias for not overlapping with tiles
        if (otz > 0 && otz < OTLEN) {
            addPrim(&ot[otz], polygt3);
        }
    }

    const auto startIndex = trianglePrims.size() * 3;
    auto& quadPrims = fm.quadPrims[currBuffer];
    const auto numQuads = quadPrims.size();
    for (std::uint16_t quadIndex = 0; quadIndex < numQuads; ++quadIndex) {
        const auto& v0 = fm.vertices[startIndex + quadIndex * 4 + 0];
        const auto& v1 = fm.vertices[startIndex + quadIndex * 4 + 1];
        const auto& v2 = fm.vertices[startIndex + quadIndex * 4 + 2];

        gte_ldv0(&v0);
        gte_ldv1(&v1);
        gte_ldv2(&v2);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        auto* polygt4 = &quadPrims[quadIndex];
        gte_stsxy0(&polygt4->x0);

        const auto& v3 = fm.vertices[startIndex + quadIndex * 4 + 3];
        gte_ldv0(&v3);
        gte_rtps();

        gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3);

        long otz;
        gte_avsz4();
        gte_stotz(&otz);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            addPrim(&ot[otz], polygt4);
        }
    }
}
