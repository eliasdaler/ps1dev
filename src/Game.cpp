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
    levelModel = loadModel("\\LEVEL.BIN;1");

    level.position = {0, 0, 0};
    level.rotation = {};
    level.scale = {ONE, ONE, ONE};

    printf("Make models...\n");
    for (int i = 0; i < numRolls; ++i) {
        rollModelInstances[i] = makeFastModelInstance(rollModel, textures[rollTextureIdx]);
    }

    roll.position = {-64, 0, 0};
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

    auto& trianglePrims = fm.trianglePrims[currBuffer];
    for (std::uint16_t triIndex = 0; triIndex < trianglePrims.size(); ++triIndex) {
        const auto& v0 = fm.vertices[triIndex * 3 + 0];
        const auto& v1 = fm.vertices[triIndex * 3 + 1];
        const auto& v2 = fm.vertices[triIndex * 3 + 2];

        gte_ldv0(&v0.x);
        gte_ldv1(&v1.x);
        gte_ldv2(&v2.x);

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
            addPrim(&ot[currBuffer][otz], polygt3);
        }
    }

    std::uint16_t startIndex = trianglePrims.size() * 3;
    auto& quadPrims = fm.quadPrims[currBuffer];
    for (std::uint16_t quadIndex = 0; quadIndex < quadPrims.size(); ++quadIndex) {
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

        const auto& v3 = fm.vertices[startIndex + quadIndex * 4 + 3];

        auto* polygt4 = &quadPrims[quadIndex];
        gte_stsxy0(&polygt4->x0);

        gte_ldv0(&v3);
        gte_rtps();

        gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3);

        long otz;
        gte_avsz4();
        gte_stotz(&otz);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            addPrim(&ot[currBuffer][otz], polygt4);
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

        setUV3(polyft3, v0.u, v0.v, v1.u, v1.v, v2.u, v2.v);

        polyft3->tpage = tpage;
        polyft3->clut = clut;

        gte_ldv0(&v0.x);
        gte_ldv1(&v1.x);
        gte_ldv2(&v2.x);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        gte_stsxy3(&polyft3->x0, &polyft3->x1, &polyft3->x2);

#if 0
        int sz1, sz2, sz3;
        __asm__ volatile("mfc2 %0, $17\n"
                         "mfc2 %1, $18\n"
                         "mfc2 %2, $19\n"
                         : "=r"(sz1), "=r"(sz2), "=r"(sz3));

        long otz = maxVar(sz1, sz2, sz3) / 4;

#else
        gte_avsz3();

        long otz;
        gte_stotz(&otz);
#endif

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

    if (subdivide) {
        struct Work {
            SVECTOR vo[4];
            SVECTOR v[4];
            CVECTOR ouv[4];
            CVECTOR uv[4];
            CVECTOR ocol[4];
            CVECTOR col[4];
            CVECTOR intCol;
        };
        auto& wrk = *(Work*)(SCRATCH_PAD + 0xC0);
        static_assert(sizeof(Work) < 200);
        static_assert(sizeof(Work) < 1024);

        const auto& texture = textures[textureIdx];
        for (int i = 0; i < mesh.numQuads; ++i, vertexIdx += 4) {
            auto* polygt4 = (POLY_GT4*)nextpri;
            setPolyGT4(polygt4);

            const auto& v0 = mesh.vertices[vertexIdx + 0];
            const auto& v1 = mesh.vertices[vertexIdx + 1];
            const auto& v2 = mesh.vertices[vertexIdx + 2];
            const auto& v3 = mesh.vertices[vertexIdx + 3];

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

            if (quad_clip(
                    &screenClip,
                    (DVECTOR*)&polygt4->x0,
                    (DVECTOR*)&polygt4->x1,
                    (DVECTOR*)&polygt4->x2,
                    (DVECTOR*)&polygt4->x3)) {
                continue;
            }

            long otz;
            gte_avsz4();
            gte_stotz(&otz);

            if (otz < 700) {
                wrk.vo[0] = SVECTOR{
                    (short)(v0.x),
                    (short)(v0.y),
                    (short)(v0.z),
                };
                wrk.vo[1] = SVECTOR{
                    (short)(v1.x),
                    (short)(v1.y),
                    (short)(v1.z),
                };
                wrk.vo[2] = SVECTOR{
                    (short)(v2.x),
                    (short)(v2.y),
                    (short)(v2.z),
                };
                wrk.vo[3] = SVECTOR{
                    (short)(v3.x),
                    (short)(v3.y),
                    (short)(v3.z),
                };

                wrk.ouv[0] = CVECTOR{v0.u, v0.v};
                wrk.ouv[1] = CVECTOR{v1.u, v1.v};
                wrk.ouv[2] = CVECTOR{v2.u, v2.v};
                wrk.ouv[3] = CVECTOR{v3.u, v3.v};

                wrk.ocol[0] = CVECTOR{v0.r, v0.g, v0.b};
                wrk.ocol[1] = CVECTOR{v1.r, v1.g, v1.b};
                wrk.ocol[2] = CVECTOR{v2.r, v2.g, v2.b};
                wrk.ocol[3] = CVECTOR{v3.r, v3.g, v3.b};

                // subdiv level 1
                long otz, p, nclip;

#define INTERP_COORD(i, a, b)                              \
    wrk.v[(i)].vx = (wrk.vo[(a)].vx + wrk.vo[(b)].vx) / 2; \
    wrk.v[(i)].vy = (wrk.vo[(a)].vy + wrk.vo[(b)].vy) / 2; \
    wrk.v[(i)].vz = (wrk.vo[(a)].vz + wrk.vo[(b)].vz) / 2;

#define INTERP_ATTR(d, i, j)                                  \
    wrk.v[(d)].vx = (wrk.vo[(i)].vx + wrk.vo[(j)].vx) / 2;    \
    wrk.v[(d)].vy = (wrk.vo[(i)].vy + wrk.vo[(j)].vy) / 2;    \
    wrk.v[(d)].vz = (wrk.vo[(i)].vz + wrk.vo[(j)].vz) / 2;    \
    wrk.uv[(d)].r = (wrk.ouv[(i)].r + wrk.ouv[(j)].r) / 2;    \
    wrk.uv[(d)].g = (wrk.ouv[(i)].g + wrk.ouv[(j)].g) / 2;    \
    wrk.col[(d)].r = (wrk.ocol[(i)].r + wrk.ocol[(j)].r) / 2; \
    wrk.col[(d)].g = (wrk.ocol[(i)].g + wrk.ocol[(j)].g) / 2; \
    wrk.col[(d)].b = (wrk.ocol[(i)].b + wrk.ocol[(j)].b) / 2;

#define COPY_ATTR(d, i)         \
    wrk.v[(d)] = wrk.vo[(i)];   \
    wrk.uv[(d)] = wrk.ouv[(i)]; \
    wrk.col[(d)] = wrk.ocol[(i)];

#define COPY_CALC_ATTR(d, i)   \
    wrk.v[(d)] = wrk.v[(i)];   \
    wrk.uv[(d)] = wrk.uv[(i)]; \
    wrk.col[(d)] = wrk.col[(i)];

#define INTERP_COLOR_GTE(i)                    \
    gte_DpqColor(&wrk.col[i], p, &wrk.intCol); \
    setRGB##i(polygt4, wrk.intCol.r, wrk.intCol.g, wrk.intCol.b);

                switch (i) {
                case 0:
                    COPY_ATTR(0, 0);
                    INTERP_ATTR(1, 0, 1);
                    INTERP_ATTR(2, 0, 2);
                    INTERP_ATTR(3, 0, 3);
                    break;
                case 1:
                    COPY_CALC_ATTR(0, 1);
                    COPY_CALC_ATTR(2, 3);
                    COPY_ATTR(1, 1);
                    INTERP_ATTR(3, 1, 3);
                    break;
                case 2:
                    COPY_CALC_ATTR(0, 2);
                    COPY_CALC_ATTR(1, 3);
                    INTERP_ATTR(2, 2, 3);
                    COPY_ATTR(3, 3);
                    break;
                case 3:
                    COPY_CALC_ATTR(1, 0);
                    COPY_CALC_ATTR(3, 2);
                    INTERP_ATTR(0, 0, 2);
                    COPY_ATTR(2, 2);
                    break;

                default:
                    __builtin_unreachable();
                    break;
                }

#define DRAW_QUAD(LABEL)                                  \
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
    gte_nclip();                                          \
    gte_stopz(&nclip);                                    \
                                                          \
    if (nclip < 0) {                                      \
        goto LABEL;                                       \
    }                                                     \
                                                          \
    gte_stsxy0(&polygt4->x0);                             \
                                                          \
    gte_ldv0(&wrk.v[3]);                                  \
    gte_rtps();                                           \
                                                          \
    gte_stsxy3(&polygt4->x1, &polygt4->x2, &polygt4->x3); \
                                                          \
    if (quad_clip(                                        \
            &screenClip,                                  \
            (DVECTOR*)&polygt4->x0,                       \
            (DVECTOR*)&polygt4->x1,                       \
            (DVECTOR*)&polygt4->x2,                       \
            (DVECTOR*)&polygt4->x3)) {                    \
        goto LABEL;                                       \
    }                                                     \
                                                          \
    gte_avsz4();                                          \
    gte_stotz(&otz);                                      \
                                                          \
    gte_stdp(&p);                                         \
                                                          \
    INTERP_COLOR_GTE(0);                                  \
    INTERP_COLOR_GTE(1);                                  \
    INTERP_COLOR_GTE(2);                                  \
    INTERP_COLOR_GTE(3);                                  \
                                                          \
    if (otz > 0 && otz < OTLEN) {                         \
        CVECTOR col;                                      \
                                                          \
        addPrim(&ot[currBuffer][otz], polygt4);           \
        nextpri += sizeof(POLY_GT4);                      \
    }

                COPY_ATTR(0, 0);
                INTERP_ATTR(1, 0, 1);
                INTERP_ATTR(2, 0, 2);
                INTERP_ATTR(3, 0, 3);
                DRAW_QUAD(quad1);

            quad1:
                COPY_CALC_ATTR(0, 1);
                COPY_CALC_ATTR(2, 3);
                COPY_ATTR(1, 1);
                INTERP_ATTR(3, 1, 3);
                DRAW_QUAD(quad2);

            quad2:
                COPY_CALC_ATTR(0, 2);
                COPY_CALC_ATTR(1, 3);
                INTERP_ATTR(2, 2, 3);
                COPY_ATTR(3, 3);
                DRAW_QUAD(quad3);

            quad3:
                COPY_CALC_ATTR(1, 0);
                COPY_CALC_ATTR(3, 2);
                INTERP_ATTR(0, 0, 2);
                COPY_ATTR(2, 2);
                DRAW_QUAD(end);
            end:
                continue;
            }

            long p;
            gte_stdp(&p);

#undef INTERP_COLOR_GTE
#define INTERP_COLOR_GTE(i)              \
    near = {v##i.r, v##i.g, v##i.b, 44}; \
    gte_DpqColor(&near, p, &col);        \
    setRGB##i(polygt4, col.r, col.g, col.b);

            CVECTOR near, col;
            INTERP_COLOR_GTE(0);
            INTERP_COLOR_GTE(1);
            INTERP_COLOR_GTE(2);
            INTERP_COLOR_GTE(3);

            otz -= 64; // depth bias for not overlapping with tiles

            if (otz > 0 && otz < OTLEN) {
                polygt4->tpage = tpage;
                polygt4->clut = clut;
                setUV4(polygt4, v0.u, v0.v, v1.u, v1.v, v2.u, v2.v, v3.u, v3.v);

                CVECTOR col;

                addPrim(&ot[currBuffer][otz], polygt4);
                nextpri += sizeof(POLY_GT4);
            }
        }
    } else {
        drawQuads(mesh, tpage, clut);
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

        setUV4(polygt4, v0.u, v0.v, v1.u, v1.v, v2.u, v2.v, v3.u, v3.v);

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

#if 0
            int sz0, sz1, sz2, sz3;
            __asm__ volatile("mfc2 %0, $16\n"
                             "mfc2 %1, $17\n"
                             "mfc2 %2, $18\n"
                             "mfc2 %3, $29\n"
                             : "=r"(sz0), "=r"(sz1), "=r"(sz2), "=r"(sz3));

            long otz = maxVar(sz0, sz1, sz2, sz3) / 4;
#else
        long otz;
        gte_avsz4();
        gte_stotz(&otz);
#endif

        long p;
        gte_stdp(&p);

#define INTERP_COLOR_GTE(i)              \
    near = {v##i.r, v##i.g, v##i.b, 44}; \
    gte_DpqColor(&near, p, &col);        \
    setRGB##i(polygt4, col.r, col.g, col.b);

        CVECTOR col, near;
        INTERP_COLOR_GTE(0);
        INTERP_COLOR_GTE(1);
        INTERP_COLOR_GTE(2);
        INTERP_COLOR_GTE(3);

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
