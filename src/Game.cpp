#include "Game.h"

#include <stdio.h>
#include <psyqo/alloc.h>
#include <psyqo/gte-registers.hh>
#include <libetc.h>
#include <libcd.h>
#include <inline_n.h>
#include <gtemac.h>
#include <string.h>

#include <utility> //std::move

#include "Utils.h"

#include "Trig.h"
#include "Clip.h"

/* scratch pad address 0x1f800000 - 0x1f800400 */
#define SCRATCH_PAD 0x1f800000
#define SCRATCH_PAD_END 0x1f800400
#define SCRATCH_PAD_SIZE 0x400

namespace
{
template<typename T>
T max(T a, T b)
{
    return a > b ? a : b;
}

template<typename T>
T maxVar(T a)
{
    return a;
}

template<typename T, typename... Args>
T maxVar(T a, Args... args)
{
    return max(maxVar(args...), a);
}

constexpr auto tileSize = 256;

bool hasCLUT(const TIM_IMAGE& texture)
{
    return texture.mode & 0x8;
}

TIM_IMAGE loadTexture(const eastl::vector<uint8_t>& timData)
{
    TIM_IMAGE texture;
    OpenTIM((u_long*)(timData.data()));
    ReadTIM(&texture);

    LoadImage(texture.prect, texture.paddr);
    DrawSync(0);

    if (hasCLUT(texture)) {
        LoadImage(texture.crect, texture.caddr);
        DrawSync(0);
    } else {
        texture.caddr = nullptr;
    }

    return texture;
}

template<unsigned... regs>
static inline void clearAllGTERegistersInternal(std::integer_sequence<unsigned, regs...> regSeq)
{
    ((psyqo::GTE::clear<psyqo::GTE::Register{regs}, psyqo::GTE::Safety::Safe>()), ...);
}

static inline void clearAllGTERegisters()
{
    clearAllGTERegistersInternal(std::make_integer_sequence<unsigned, 64>{});
}

}

void Game::init()
{
    clearAllGTERegisters();

    PadInit(0);
    ResetGraph(0);

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

    SetDispMask(1); // Display on screen

    // setRGB0(&drawEnv[0], 100, 149, 237);
    // setRGB0(&drawEnv[1], 100, 149, 237);

    setRGB0(&drawEnv[0], 0, 0, 0);
    setRGB0(&drawEnv[1], 0, 0, 0);

    drawEnv[0].isbg = 1;
    drawEnv[1].isbg = 1;

    PutDispEnv(&dispEnv[currBuffer]);
    PutDrawEnv(&drawEnv[currBuffer]);

    FntLoad(960, 0);
    FntOpen(16, 16, 196, 64, 0, 256);

    setVector(&camera.position, 0, -ONE * (tileSize + tileSize / 5), -ONE * 2500);
    camera.position.vz = ONE * -376;

    // testing
    /* camera.position.vx = ONE * -379;
    camera.position.vy = ONE * -307;
    camera.position.vz = ONE * -3496;
    camera.rotation.vx = ONE * 64;
    camera.rotation.vy = ONE * -236; */

    CdInit();

    const auto textureData2 = util::readFile("\\BRICKS.TIM;1");
    bricksTextureIdx = addTexture(loadTexture(textureData2));

    const auto textureData3 = util::readFile("\\ROLL.TIM;1");
    rollTextureIdx = addTexture(loadTexture(textureData3));

    printf("Load models...\n");
    rollModel = loadFastModel("\\ROLL.FM;1");
    printf("sizeof Vertex: %d", (int)sizeof(FastVertex));
    levelModel = loadModel("\\LEVEL.BIN;1");

    level.position = {0, 0, 0};
    level.rotation = {};
    level.scale = {ONE, ONE, ONE};

    printf("Make models...\n");
    for (int i = 0; i < numRolls; ++i) {
        rollModelInstances[i] = makeFastModelInstance(rollModel, textures[rollTextureIdx]);
    }

    roll.position = {-1024, 0, 0};
    roll.rotation = {};
    roll.scale = {ONE, ONE, ONE};

    printf("Init done...\n");
}

void Game::run()
{
    while (true) {
        handleInput();
        update();
        draw();
    }
}

void Game::handleInput()
{
    trot.vx = camera.rotation.vx >> 12;
    trot.vy = camera.rotation.vy >> 12;
    trot.vz = camera.rotation.vz >> 12;

    const auto PadStatus = PadRead(0);

    // rotating the camera around Y
    if (PadStatus & PADLleft) {
        camera.rotation.vy += ONE * 25;
    }
    if (PadStatus & PADLright) {
        camera.rotation.vy -= ONE * 25;
    }

    // strafing
    if (PadStatus & PADL1) {
        camera.position.vx -= math::icos(trot.vy) << 4;
        camera.position.vz -= math::isin(trot.vy) << 4;
    }
    if (PadStatus & PADR1) {
        camera.position.vx += math::icos(trot.vy) << 4;
        camera.position.vz += math::isin(trot.vy) << 4;
    }

    // looking up/down
    if (PadStatus & PADL2) {
        camera.rotation.vx += ONE * 10;
    }
    if (PadStatus & PADR2) {
        camera.rotation.vx -= ONE * 10;
    }

    // Moving forwards/backwards
    if (PadStatus & PADLup) {
        camera.position.vx -= math::isin(trot.vy) << 5;
        camera.position.vz += math::icos(trot.vy) << 5;
    }
    if (PadStatus & PADLdown) {
        camera.position.vx += math::isin(trot.vy) << 5;
        camera.position.vz -= math::icos(trot.vy) << 5;
    }
}

void Game::update()
{}

void Game::drawModel(Object& object, const Model& model, std::uint16_t textureIdx, bool subdivide)
{
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    for (const auto& mesh : model.meshes) {
        drawMesh(object, mesh, textureIdx, subdivide);
    }
}

void Game::drawModelFast(Object& object, const FastModelInstance& fm)
{
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    auto& ot = this->ot[currBuffer];
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

void Game::drawMesh(Object& object, const Mesh& mesh, std::uint16_t textureIdx, bool subdivide)
{
    const auto& texture = textures[textureIdx];
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

            addPrim(&ot[currBuffer][otz], polyft3);
            nextpri += sizeof(POLY_FT3);
        }
    }

    if (!subdivide) {
        drawQuads(mesh, tpage, clut);
        return;
    }

    auto& wrk1 = *(Work*)(SCRATCH_PAD + 0xC0);

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
                                                              \
    if (otz > 0 && otz < OTLEN) {                             \
        gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3); \
        gte_stdp(&p);                                         \
                                                              \
        INTERP_COLOR_GTE(wrk, 0);                             \
        INTERP_COLOR_GTE(wrk, 1);                             \
        INTERP_COLOR_GTE(wrk, 2);                             \
        INTERP_COLOR_GTE(wrk, 3);                             \
                                                              \
        addPrim(&ot[currBuffer][otz], polygt4);               \
        nextpri += sizeof(POLY_GT4);                          \
    }

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
            long otz, p, nclip;

            auto& wrk2 = *reinterpret_cast<Work2*>(&wrk1);

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
            long otz, p, nclip;
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

            CVECTOR col;

            addPrim(&ot[currBuffer][otz], polygt4);
            nextpri += sizeof(POLY_GT4);
        }
    }
}

void Game::drawQuads(const Mesh& mesh, u_long tpage, int clut)
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

#define INTERP_COLOR_GTE3(i)                         \
    near = {v##i.col.r, v##i.col.g, v##i.col.b, 44}; \
    gte_DpqColor(&near, p, &col);                    \
    setRGB##i(polygt4, col.r, col.g, col.b);

        CVECTOR col, near;
        INTERP_COLOR_GTE3(0);
        INTERP_COLOR_GTE3(1);
        INTERP_COLOR_GTE3(2);
        INTERP_COLOR_GTE3(3);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            CVECTOR col;

            addPrim(&ot[currBuffer][otz], polygt4);
            nextpri += sizeof(POLY_GT4);
        }
    }
}

void Game::draw()
{
    ClearOTagR(ot[currBuffer], OTLEN);
    nextpri = primbuff[currBuffer];

    // camera.position.vx = cube.position.vx;
    // camera.position.vz = cube.position.vz - 32;

    // VECTOR globalUp{0, -ONE, 0};
    // camera::lookAt(camera, camera.position, cube.position, globalUp);

    { // fps camera
        RotMatrix(&trot, &camera.view);

        tpos.vx = -camera.position.vx >> 12;
        tpos.vy = -camera.position.vy >> 12;
        tpos.vz = -camera.position.vz >> 12;

        ApplyMatrixLV(&camera.view, &tpos, &tpos);
        TransMatrix(&camera.view, &tpos);
    }

    auto pos = roll.position;
    drawModelFast(roll, rollModelInstances[0]);

    drawModel(level, levelModel, bricksTextureIdx, true);

    for (int i = 1; i < numRolls; ++i) {
        roll.position.vx += 128;
        drawModelFast(roll, rollModelInstances[i]);
    }

    roll.position = pos;

    FntPrint(
        "X=%d Y=%d Z=%d\n",
        camera.position.vx >> 12,
        camera.position.vy >> 12,
        camera.position.vz >> 12);
    FntPrint("RX=%d, RY=%d\n", trot.vx, trot.vy);

    FntFlush(-1);

    display();
}

void Game::display()
{
    DrawSync(0);
    VSync(0);
    PutDispEnv(&dispEnv[currBuffer]);
    PutDrawEnv(&drawEnv[currBuffer]);
    DrawOTag(&ot[currBuffer][OTLEN - 1]);
    currBuffer = !currBuffer;
}

std::uint16_t Game::addTexture(TIM_IMAGE texture)
{
    auto id = textures.size();
    textures.push_back(std::move(texture));
    return static_cast<std::uint16_t>(id);
}
