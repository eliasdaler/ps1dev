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

void Renderer::init()
{
    InitGeom();
    SetGeomOffset(SCREENXRES / 2, SCREENYRES / 2);
    SetGeomScreen(420); // fov

    SetBackColor(0, 0, 0);
    SetFarColor(0, 0, 0);
    SetFogNearFar(500, 12800, SCREENXRES / 2);

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

    long otz, p, nclip;

    std::size_t vertexIdx = 0;

    // TODO: subdiv for triangles
    for (std::size_t i = 0; i < mesh.numTris; ++i, vertexIdx += 3) {
        // FIXME: should be POLY_GT3!!!
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
        gte_stopz(&nclip);
        if (nclip < 0) {
            continue;
        }

        gte_stsxy3(&polyft3->x0, &polyft3->x1, &polyft3->x2);

        gte_avsz3();

        gte_stotz(&otz);

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
    auto& wrk2 = *reinterpret_cast<SubdivData2*>(&wrk1);

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

#define INTERP_COLOR_GTE2(i)          \
    gte_DpqColor(&v##i.col, p, &col); \
    setRGB##i(polygt4, col.r, col.g, col.b);

        CVECTOR col;
        gte_stdp(&p);
        INTERP_COLOR_GTE2(0);
        INTERP_COLOR_GTE2(1);
        INTERP_COLOR_GTE2(2);
        INTERP_COLOR_GTE2(3);

        otz -= 64; // depth bias for not overlapping with tiles
        if (otz > 0 && otz < OTLEN) {
            polygt4->tpage = tpage;
            polygt4->clut = clut;
            setUV4(polygt4, v0.uv.r, v0.uv.g, v1.uv.r, v1.uv.g, v2.uv.r, v2.uv.g, v3.uv.r, v3.uv.g);

            addPrim(&ot[otz], polygt4);
            nextpri += sizeof(POLY_GT4);
        }
    }
}

void Renderer::drawQuads(const Mesh& mesh, u_long tpage, int clut)
{
    long otz, nclip, p;

    for (int i = 0, vertexIdx = mesh.numTris * 3; i < mesh.numQuads; ++i, vertexIdx += 4) {
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

        gte_avsz4();
        gte_stotz(&otz);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            gte_stdp(&p);

#define INTERP_COLOR_GTE3(i)          \
    gte_DpqColor(&v##i.col, p, &col); \
    setRGB##i(polygt4, col.r, col.g, col.b);

            CVECTOR col;
            INTERP_COLOR_GTE3(0);
            INTERP_COLOR_GTE3(1);
            INTERP_COLOR_GTE3(2);
            INTERP_COLOR_GTE3(3);

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

        otz -= 64; // depth bias for not overlapping with tiles
        if (otz > 0 && otz < OTLEN) {
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

        otz -= 64; // depth bias for not overlapping with tiles
        if (otz > 0 && otz < OTLEN) {
            addPrim(&ot[otz], polygt4);
        }
    }
}
