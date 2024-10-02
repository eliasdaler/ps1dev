#include "Renderer.h"

#include "Clip.h"
#include "FastModel.h"
#include "Model.h"
#include "Object.h"
#include "Camera.h"
#include "Globals.h"

#include "Subdivision.h"

#include <inline_n.h>
#include <gtemac.h>
#include <libetc.h>

namespace
{
inline constexpr int LEVEL_1_SUBDIV_DIST = 700;
inline constexpr int LEVEL_2_SUBDIV_DIST = 300;
}

#define INTERP_COLOR_GTE(prim, i)     \
    gte_DpqColor(&v##i.col, p, &col); \
    setRGB##i((prim), col.r, col.g, col.b);

void Renderer::init()
{
    ResetGraph(0);
    InitGeom();
    SetGeomOffset(SCREENXRES / 2, SCREENYRES / 2);
    SetGeomScreen(420); // fov
    SetGeomScreen(300); // fov

    SetBackColor(0, 0, 0);
    SetFarColor(0, 0, 0);
    SetFogNearFar(5000, 20800, SCREENXRES / 2);

    SetDefDispEnv(&dispEnv[0], 0, 0, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&drawEnv[0], 0, SCREENYRES, SCREENXRES, SCREENYRES);

    SetDefDispEnv(&dispEnv[1], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&drawEnv[1], 0, 0, SCREENXRES, SCREENYRES);

    setRECT(&screenClip, 0, 0, SCREENXRES, SCREENYRES);

    bool palMode = false;
    if (palMode) {
        SetVideoMode(MODE_PAL);
        dispEnv[0].screen.y += 8;
        dispEnv[1].screen.y += 8;
    }
    currBuffer = 0;

    SetDispMask(1); // Display on screen

    // set BG color
    // setRGB0(&drawEnv[0], 100, 149, 237);
    // setRGB0(&drawEnv[1], 100, 149, 237);
    setRGB0(&drawEnv[0], 0, 0, 0);
    setRGB0(&drawEnv[1], 0, 0, 0);

    drawEnv[0].isbg = 1;
    drawEnv[1].isbg = 1;

    PutDispEnv(&dispEnv[currBuffer]);
    PutDrawEnv(&drawEnv[currBuffer]);

    // load default debug font
    FntLoad(960, 0);
    FntOpen(16, 16, 196, 64, 0, 256);
}

void Renderer::beginDraw()
{
    ot = ots[currBuffer];

    ClearOTagR(ot, OTLEN);
    nextpri = primbuff[currBuffer];

    // VECTOR globalUp{0, -ONE, 0};
    // camera::lookAt(camera, camera.position, cube.position, globalUp);

    { // fps camera
        RotMatrix(&camera.trot, &camera.view);

        camera.tpos.vx = -camera.position.vx >> 12;
        camera.tpos.vy = -camera.position.vy >> 12;
        camera.tpos.vz = -camera.position.vz >> 12;

        ApplyMatrixLV(&camera.view, &camera.tpos, &camera.tpos);
        TransMatrix(&camera.view, &camera.tpos);
    }
}

void Renderer::display()
{
    DrawSync(0);
    VSync(0);
    PutDispEnv(&dispEnv[currBuffer]);
    PutDrawEnv(&drawEnv[currBuffer]);
    DrawOTag(&ot[OTLEN - 1]);

    currBuffer = !currBuffer;
}

void Renderer::drawModel(Object& object, const Model& model, TIM_IMAGE& texture, int depthBias)
{
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    for (const auto& mesh : model.meshes) {
        drawMesh(object, mesh, texture, mesh.subdivide, depthBias);
    }
}

void Renderer::drawMesh(
    Object& object,
    const Mesh& mesh,
    const TIM_IMAGE& texture,
    bool subdivide,
    int depthBias)
{
    const auto tpage = getTPage(texture.mode & 0x3, 0, texture.prect->x, texture.prect->y);
    const auto clut = getClut(texture.crect->x, texture.crect->y);

    long otz, p, nclip;

    std::size_t vertexIdx = 0;

    // untextured faces don't need subdiv
    vertexIdx = drawTris<POLY_G3>(mesh, tpage, clut, mesh.numUntexturedTris, vertexIdx, depthBias);
    if (!subdivide) {
        vertexIdx =
            drawQuads<POLY_G4>(mesh, tpage, clut, mesh.numUntexturedQuads, vertexIdx, depthBias);
    } else {
        vertexIdx = drawQuadsSubdiv<
            POLY_G4>(mesh, tpage, clut, mesh.numUntexturedQuads, vertexIdx, depthBias);
    }

    // TODO: subdiv for triangles
    vertexIdx = drawTris<POLY_GT3>(mesh, tpage, clut, mesh.numTris, vertexIdx, depthBias);

    if (!subdivide) {
        vertexIdx = drawQuads<POLY_GT4>(mesh, tpage, clut, mesh.numQuads, vertexIdx, depthBias);
    } else {
        vertexIdx =
            drawQuadsSubdiv<POLY_GT4>(mesh, tpage, clut, mesh.numQuads, vertexIdx, depthBias);
    }
}

template<typename PrimType>
int Renderer::drawTris(
    const Mesh& mesh,
    u_long tpage,
    int clut,
    int numFaces,
    int vertexIdx,
    int depthBias)
{
    long otz, nclip, p;

    for (int i = 0; i < numFaces; ++i, vertexIdx += 3) {
        auto* polyg3 = (PrimType*)nextpri;
        if constexpr (eastl::is_same_v<PrimType, POLY_GT3>) {
            setPolyGT3(polyg3);
        } else {
            static_assert(eastl::is_same_v<PrimType, POLY_G3>);
            setPolyG3(polyg3);
        }

        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];

        if constexpr (eastl::is_same_v<PrimType, POLY_GT3>) {
            setUV3(polyg3, v0.uv.r, v0.uv.g, v1.uv.r, v1.uv.g, v2.uv.r, v2.uv.g);

            polyg3->tpage = tpage;
            polyg3->clut = clut;
        }

        gte_ldv0(&v0);
        gte_ldv1(&v1);
        gte_ldv2(&v2);

        gte_rtpt();

        gte_nclip();
        gte_stopz(&nclip);
        if (nclip < 0) {
            continue;
        }

        gte_stsxy3(&polyg3->x0, &polyg3->x1, &polyg3->x2);

        gte_avsz3();
        gte_stotz(&otz);
        otz -= depthBias;
        if (otz > 0 && otz < OTLEN) {
            gte_stdp(&p);

            CVECTOR col;
            INTERP_COLOR_GTE(polyg3, 0);
            INTERP_COLOR_GTE(polyg3, 1);
            INTERP_COLOR_GTE(polyg3, 2);

            addPrim(&ot[otz], polyg3);
            nextpri += sizeof(PrimType);
        }
    }

    return vertexIdx;
}

template<typename PrimType>
int Renderer::drawQuads(
    const Mesh& mesh,
    u_long tpage,
    int clut,
    int numFaces,
    int vertexIdx,
    int depthBias)
{
    long otz, nclip, p;

    for (int i = 0; i < numFaces; ++i, vertexIdx += 4) {
        auto* polyg4 = (PrimType*)nextpri;
        if constexpr (eastl::is_same_v<PrimType, POLY_GT4>) {
            setPolyGT4(polyg4);
        } else {
            static_assert(eastl::is_same_v<PrimType, POLY_G4>);
            setPolyG4(polyg4);
        }

        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];
        const auto& v3 = mesh.vertices[vertexIdx + 3];

        if constexpr (eastl::is_same_v<PrimType, POLY_GT4>) {
            setUV4(polyg4, v0.uv.r, v0.uv.g, v1.uv.r, v1.uv.g, v2.uv.r, v2.uv.g, v3.uv.r, v3.uv.g);

            polyg4->tpage = tpage;
            polyg4->clut = clut;
        }

        gte_ldv0(&v0);
        gte_ldv1(&v1);
        gte_ldv2(&v2);

        gte_rtpt();

        gte_nclip();
        gte_stopz(&nclip);
        if (nclip < 0) {
            continue;
        }

        gte_stsxy0(&polyg4->x0);

        gte_ldv0(&v3);
        gte_rtps();

        gte_stsxy3(&polyg4->x1, &polyg4->x2, &polyg4->x3);

        /* if (quad_clip(
                &screenClip,
                (DVECTOR*)&polyft4->x0,
                (DVECTOR*)&polyft4->x1,
                (DVECTOR*)&polyft4->x2,
                (DVECTOR*)&polyft4->x3)) {
            return;
        } */

        gte_avsz4();
        gte_stotz(&otz);
        otz -= depthBias;
        if (otz > 0 && otz < OTLEN) {
            gte_stdp(&p);

            CVECTOR col;
            INTERP_COLOR_GTE(polyg4, 0);
            INTERP_COLOR_GTE(polyg4, 1);
            INTERP_COLOR_GTE(polyg4, 2);
            INTERP_COLOR_GTE(polyg4, 3);

            addPrim(&ot[otz], polyg4);
            nextpri += sizeof(PrimType);
        }
    }

    return vertexIdx;
}

void Renderer::drawModelFast(Object& object, const FastModelInstance& fm, int depthBias)
{
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    long otz, p, nclip;

    const auto& trianglePrims = fm.trianglePrims[currBuffer];
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
        gte_stopz(&nclip);
        if (nclip < 0) {
            continue;
        }

        auto* polygt3 = &trianglePrims[triIndex];
        gte_stsxy3(&polygt3->x0, &polygt3->x1, &polygt3->x2);

        gte_avsz3();
        gte_stotz(&otz);
        otz -= depthBias;
        if (otz > 0 && otz < OTLEN) {
            gte_stdp(&p);

            CVECTOR col;
            INTERP_COLOR_GTE(polygt3, 0);
            INTERP_COLOR_GTE(polygt3, 1);
            INTERP_COLOR_GTE(polygt3, 2);

            addPrim(&ot[otz], polygt3);
        }
    }

    const auto startIndex = trianglePrims.size() * 3;
    const auto& quadPrims = fm.quadPrims[currBuffer];
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

        gte_avsz4();
        gte_stotz(&otz);
        otz -= 64; // depth bias
        if (otz > 0 && otz < OTLEN) {
            gte_stdp(&p);

            CVECTOR col;
            INTERP_COLOR_GTE(polygt4, 0);
            INTERP_COLOR_GTE(polygt4, 1);
            INTERP_COLOR_GTE(polygt4, 2);
            INTERP_COLOR_GTE(polygt4, 3);

            addPrim(&ot[otz], polygt4);
        }
    }
}

template<typename PrimType>
int Renderer::drawQuadsSubdiv(
    const Mesh& mesh,
    u_long tpage,
    int clut,
    int numFaces,
    int vertexIdx,
    int depthBias)
{
    long otz, p, nclip;

    auto& wrk1 = *(SubdivData1*)(SCRATCH_PAD + 0xC0);
    auto& wrk2 = *reinterpret_cast<SubdivData2*>(&wrk1);

    for (int i = 0; i < numFaces; ++i, vertexIdx += 4) {
        auto* polyg4 = (PrimType*)nextpri;
        if constexpr (eastl::is_same_v<PrimType, POLY_GT4>) {
            setPolyGT4(polyg4);
        } else {
            static_assert(eastl::is_same_v<PrimType, POLY_G4>);
            setPolyG4(polyg4);
        }

        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];
        const auto& v3 = mesh.vertices[vertexIdx + 3];

        gte_ldv0(&v0.pos);
        gte_ldv1(&v1.pos);
        gte_ldv2(&v2.pos);

        gte_rtpt();

        gte_nclip();

        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        gte_stsxy0(&polyg4->x0);

        gte_ldv0(&v3.pos);
        gte_rtps();

        gte_stsxy3(&polyg4->x1, &polyg4->x2, &polyg4->x3);

        // FIXME: pretty slow for many quads, need to call less often?
        if (quad_clip(
                screenClip,
                (const DVECTOR&)polyg4->x0,
                (const DVECTOR&)polyg4->x1,
                (const DVECTOR&)polyg4->x2,
                (const DVECTOR&)polyg4->x3)) {
            continue;
        }

        gte_avsz4();
        gte_stotz(&otz);

        if (otz < LEVEL_1_SUBDIV_DIST) {
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
        }

        if (otz < LEVEL_2_SUBDIV_DIST) {
            for (int i = 0; i < 4; ++i) {
                wrk2.oov[i] = wrk2.ov[i];
                wrk2.oouv[i] = wrk2.ouv[i];
                wrk2.oocol[i] = wrk2.ocol[i];
            }

            DRAW_QUADS_44(wrk2);
            continue;
        }

        if (otz < LEVEL_1_SUBDIV_DIST) {
            DRAW_QUADS_22(wrk1);
            continue;
        }

        // else: draw without subdiv

        CVECTOR col;
        gte_stdp(&p);
        INTERP_COLOR_GTE(polyg4, 0);
        INTERP_COLOR_GTE(polyg4, 1);
        INTERP_COLOR_GTE(polyg4, 2);
        INTERP_COLOR_GTE(polyg4, 3);

        otz -= depthBias;
        if (otz > 0 && otz < OTLEN) {
            if constexpr (eastl::is_same_v<PrimType, POLY_GT4>) {
                polyg4->tpage = tpage;
                polyg4->clut = clut;
                setUV4(
                    polyg4, v0.uv.r, v0.uv.g, v1.uv.r, v1.uv.g, v2.uv.r, v2.uv.g, v3.uv.r, v3.uv.g);
            }

            addPrim(&ot[otz], polyg4);
            nextpri += sizeof(POLY_GT4);
        }
    }
    return vertexIdx;
}
