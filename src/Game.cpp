#include "Game.h"

#include <stdio.h>
#include <libetc.h>
#include <libcd.h>
#include <inline_n.h>
#include <gtemac.h>

#include <utility> //std::move

#include "Utils.h"

#include "Trig.h"
#include "Clip.h"

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

}

void Game::init()
{
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
    // camera.position.vy = -ONE * 51;
    camera.view = (MATRIX){0};

    // testing
    /* camera.position.vx = ONE * 79;
    camera.position.vy = ONE * -307;
    camera.position.vz = ONE * -1538;
    camera.rotation.vx = ONE * 250;
    camera.rotation.vy = ONE * 275; */

    CdInit();

    const auto textureData2 = util::readFile("\\BRICKS.TIM;1");
    bricksTextureIdx = addTexture(loadTexture(textureData2));

    const auto textureData = util::readFile("\\FLOOR.TIM;1");
    floorTextureIdx = addTexture(loadTexture(textureData));

    const auto textureData4 = util::readFile("\\PS1.TIM;1");
    rollTextureIdx = addTexture(loadTexture(textureData4));

    const auto textureData3 = util::readFile("\\ROLL.TIM;1");
    rollTextureIdx = addTexture(loadTexture(textureData3));

    loadModel(rollModel, "\\ROLL.BIN;1");

    roll.position = {-64, 0, 0};
    roll.rotation = {};
    roll.scale = {ONE, ONE, ONE};

    loadModel(levelModel, "\\LEVEL.BIN;1");
    level.position = {};
    level.rotation = {};
    level.scale = {ONE, ONE, ONE};

    Mesh floorTile;
    floorTile.vertices = {
        {-tileSize, 0, tileSize},
        {tileSize, 0, tileSize},
        {-tileSize, 0, -tileSize},
        {tileSize, 0, -tileSize},
    };
    floorMeshIdx = addMesh(std::move(floorTile));

    floorTileObj.position = {0, 0, 0};
    floorTileObj.rotation = {};
    floorTileObj.scale = {ONE, ONE, ONE};

    Mesh wallTileMesh;
    wallTileMesh.vertices = {
        {-tileSize, -tileSize, 0},
        {tileSize, -tileSize, 0},
        {-tileSize, tileSize, 0},
        {tileSize, tileSize, 0},
    };
    wallMeshIdx = addMesh(std::move(wallTileMesh));

    Mesh wallTileMeshL;
    wallTileMeshL.vertices = {
        {0, -tileSize, -tileSize},
        {0, -tileSize, tileSize},
        {0, tileSize, -tileSize},
        {0, tileSize, tileSize},
    };
    wallMeshLIdx = addMesh(std::move(wallTileMeshL));

    Mesh wallTileMeshR;
    wallTileMeshR.vertices = {
        {0, -tileSize, tileSize},
        {0, -tileSize, -tileSize},
        {0, tileSize, tileSize},
        {0, tileSize, -tileSize},
    };
    wallMeshRIdx = addMesh(std::move(wallTileMeshR));

    wallTileObj.position = {0, 0, 0};
    wallTileObj.rotation = {};
    wallTileObj.scale = {ONE, ONE, ONE};

    tileMesh.vertices.resize(4);
}

void Game::loadModel(Model& model, eastl::string_view filename)
{
    const auto data = util::readFile(filename);
    const auto* bytes = (u_char*)data.data();

    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numSubmeshes = fr.GetUInt16();
    Vertex vertex;
    for (int i = 0; i < numSubmeshes; ++i) {
        Mesh mesh;

        mesh.numTris = fr.GetUInt16();
        for (int j = 0; j < mesh.numTris; ++j) {
            for (int k = 0; k < 3; ++k) {
                vertex.x = fr.GetInt16();
                vertex.y = fr.GetInt16();
                vertex.z = fr.GetInt16();
                vertex.u = fr.GetUInt8();
                vertex.v = fr.GetUInt8();
                mesh.vertices.push_back(vertex);
            }
        }

        mesh.numQuads = fr.GetUInt16();
        for (int j = 0; j < mesh.numQuads; ++j) {
            for (int k = 0; k < 4; ++k) {
                vertex.x = fr.GetInt16();
                vertex.y = fr.GetInt16();
                vertex.z = fr.GetInt16();
                vertex.u = fr.GetUInt8();
                vertex.v = fr.GetUInt8();
                mesh.vertices.push_back(vertex);
            }
        }

        model.meshesIndices.push_back(addMesh(std::move(mesh)));
    }
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

void Game::drawModel(Object& object, const Model& model, std::uint16_t textureIdx)
{
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    for (const auto& meshIdx : model.meshesIndices) {
        drawMesh(object, meshes[meshIdx], textureIdx);
    }
}

void Game::drawMesh(Object& object, const Mesh& mesh, std::uint16_t textureIdx)
{
    const auto& texture = textures[textureIdx];
    const auto tpage = getTPage(texture.mode & 0x3, 0, texture.prect->x, texture.prect->y);
    const auto clut = getClut(texture.crect->x, texture.crect->y);

    std::size_t vertexIdx = 0;

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

#if 1
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

    for (int i = 0; i < mesh.numQuads; ++i, vertexIdx += 4) {
        auto* polyft4 = (POLY_FT4*)nextpri;
        setPolyFT4(polyft4);

        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];
        const auto& v3 = mesh.vertices[vertexIdx + 3];

        setUV4(polyft4, v0.u, v0.v, v1.u, v1.v, v2.u, v2.v, v3.u, v3.v);

        polyft4->tpage = tpage;
        polyft4->clut = clut;

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

        gte_stsxy0(&polyft4->x0);

        gte_ldv0(&v3);
        gte_rtps();

        gte_stsxy3(&polyft4->x1, &polyft4->x2, &polyft4->x3);

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

        CVECTOR near = {128, 128, 128, 44};
        CVECTOR col;
        gte_DpqColor(&near, p, &col);
        setRGB0(polyft4, col.r, col.g, col.b);

        otz -= 64; // depth bias for not overlapping with tiles

        if (otz > 0 && otz < OTLEN) {
            CVECTOR col;

            addPrim(&ot[currBuffer][otz], polyft4);
            nextpri += sizeof(POLY_FT4);
        }
    }
}

void Game::drawTile(
    Object& object,
    std::uint16_t meshIdx,
    std::uint16_t textureIdx,
    const TexRegion& uvs)
{
    auto& protoMesh = meshes[meshIdx];
    Quad quad{};
    for (int i = 0; i < protoMesh.vertices.size(); ++i) {
        quad.vs[i].vx = protoMesh.vertices[i].x + object.position.vx;
        quad.vs[i].vy = protoMesh.vertices[i].y + object.position.vy;
        quad.vs[i].vz = protoMesh.vertices[i].z + object.position.vz;
    }

    auto& texture = textures[textureIdx];

    auto depth = 0;
    auto cpos =
        VECTOR{camera.position.vx >> 12, camera.position.vy >> 12, camera.position.vz >> 12};
    auto dist = (cpos.vx - object.position.vx) * (cpos.vx - object.position.vx) +
                (cpos.vy - object.position.vy) * (cpos.vy - object.position.vy) +
                (cpos.vz - object.position.vz) * (cpos.vz - object.position.vz);

    dist >>= 12;
    if (dist == 0) {
        dist = 1;
    }
    if (meshIdx != floorMeshIdx) {
        if (dist < 250) {
            depth = 2;
        } else if (dist < 600) {
            depth = 1;
        }
    } else {
        // floor - TODO
        // not subdividing it is a bit nicer since we don't get holes
        // the distortion is not as apparent, unless you look down
        /* if (trot.vx > 180) { // only subdivide when looking down
            if (dist < 100) {
                depth = 2;
            }
        } */
    }

    RotMatrix(&object.rotation, &worldmat);
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    gte_SetRotMatrix(&viewmat);
    gte_SetTransMatrix(&viewmat);

    drawQuadRecursive(object, quad, uvs, texture, depth);
}

void Game::drawQuadRecursive(
    Object& object,
    const Quad& quad,
    const TexRegion& uvs,
    const TIM_IMAGE& texture,
    int depth)
{
    if (depth != 0) {
        const auto tu0 = uvs.u0;
        const auto tv0 = uvs.v0;
        const auto tu1 = uvs.u3;
        const auto tv1 = uvs.v0;
        const auto tu2 = uvs.u0;
        const auto tv2 = uvs.v3;
        const auto tu3 = uvs.u3;
        const auto tv3 = uvs.v3;

        const auto tum01 = (tu0 + tu1) / 2;
        const auto tvm01 = (tv0 + tv1) / 2;
        const auto tum02 = (tu0 + tu2) / 2;
        const auto tvm02 = (tv0 + tv2) / 2;
        const auto tum03 = (tu0 + tu3) / 2;
        const auto tvm03 = (tv0 + tv3) / 2;
        const auto tum12 = (tu1 + tu2) / 2;
        const auto tvm12 = (tv1 + tv2) / 2;
        const auto tum13 = (tu1 + tu3) / 2;
        const auto tvm13 = (tv1 + tv3) / 2;
        const auto tum23 = (tu2 + tu3) / 2;
        const auto tvm23 = (tv2 + tv3) / 2;

        const auto v01 = SVECTOR{
            (short)((quad.vs[0].vx + quad.vs[1].vx) / 2),
            (short)((quad.vs[0].vy + quad.vs[1].vy) / 2),
            (short)((quad.vs[0].vz + quad.vs[1].vz) / 2),
        };
        const auto v02 = SVECTOR{
            (short)((quad.vs[0].vx + quad.vs[2].vx) / 2),
            (short)((quad.vs[0].vy + quad.vs[2].vy) / 2),
            (short)((quad.vs[0].vz + quad.vs[2].vz) / 2),
        };
        const auto v03 = SVECTOR{
            (short)((quad.vs[0].vx + quad.vs[3].vx) / 2),
            (short)((quad.vs[0].vy + quad.vs[3].vy) / 2),
            (short)((quad.vs[0].vz + quad.vs[3].vz) / 2),
        };
        const auto v13 = SVECTOR{
            (short)((quad.vs[1].vx + quad.vs[3].vx) / 2),
            (short)((quad.vs[1].vy + quad.vs[3].vy) / 2),
            (short)((quad.vs[1].vz + quad.vs[3].vz) / 2),
        };
        const auto v23 = SVECTOR{
            (short)((quad.vs[2].vx + quad.vs[3].vx) / 2),
            (short)((quad.vs[2].vy + quad.vs[3].vy) / 2),
            (short)((quad.vs[2].vz + quad.vs[3].vz) / 2),
        };

        Quad quadDiv;
        TexRegion tr;

        // top left
        quadDiv = {quad.vs[0], v01, v02, v03};
        tr = {
            .u0 = tu0,
            .v0 = tv1,
            .u3 = tum03,
            .v3 = tvm03,
        };
        drawQuadRecursive(object, quadDiv, tr, texture, depth - 1);

        // top right
        quadDiv = {v01, quad.vs[1], v03, v13};
        tr = {
            .u0 = tum01,
            .v0 = tvm01,
            .u3 = tum13,
            .v3 = tvm13,
        };
        drawQuadRecursive(object, quadDiv, tr, texture, depth - 1);

        // bottom keft
        quadDiv = {v02, v03, quad.vs[2], v23};
        tr = {
            .u0 = tum02,
            .v0 = tvm02,
            .u3 = tum23,
            .v3 = tvm23,
        };
        drawQuadRecursive(object, quadDiv, tr, texture, depth - 1);

        // bottom right
        quadDiv = {v03, v13, v23, quad.vs[3]};
        tr = {
            .u0 = tum03,
            .v0 = tvm03,
            .u3 = tu3,
            .v3 = tv3,
        };
        drawQuadRecursive(object, quadDiv, tr, texture, depth - 1);
    } else if (depth == 0) {
        auto* polyft4 = (POLY_FT4*)nextpri;
        setPolyFT4(polyft4);

        setUV4(polyft4, uvs.u0, uvs.v0, uvs.u3, uvs.v0, uvs.u0, uvs.v3, uvs.u3, uvs.v3);

        polyft4->tpage = getTPage(texture.mode & 0x3, 0, texture.prect->x, texture.prect->y);
        polyft4->clut = getClut(texture.crect->x, texture.crect->y);

        gte_ldv0(&quad.vs[0]);
        gte_ldv1(&quad.vs[1]);
        gte_ldv2(&quad.vs[2]);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            return;
        }

        gte_stsxy0(&polyft4->x0);

        gte_ldv0(&quad.vs[3]);
        gte_rtps();

        gte_stsxy3(&polyft4->x1, &polyft4->x2, &polyft4->x3);

        if (quad_clip(
                &screenClip,
                (DVECTOR*)&polyft4->x0,
                (DVECTOR*)&polyft4->x1,
                (DVECTOR*)&polyft4->x2,
                (DVECTOR*)&polyft4->x3)) {
            return;
        }

        long otz;
        gte_avsz4();
        gte_stotz(&otz);

        long p;
        gte_stdp(&p);

        CVECTOR near = {128, 128, 128, 44};
        CVECTOR col;
        gte_DpqColor(&near, p, &col);
        setRGB0(polyft4, col.r, col.g, col.b);

        if (otz > 0 && otz < OTLEN) {
            CVECTOR col;

            addPrim(&ot[currBuffer][otz], polyft4);
            nextpri += sizeof(POLY_FT4);
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

    // drawLevel();

    auto pos = roll.position;
    drawModel(roll, rollModel, rollTextureIdx);

    drawModel(level, levelModel, bricksTextureIdx);

#if 0
    for (int i = 0; i < 10; ++i) {
        roll.position.vx += 128;
        drawModel(roll, rollModel, rollTextureIdx);
    }
#endif

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

void Game::drawLevel()
{
    const auto tileUV = TexRegion{0, 0, 63, 63};
    const auto windowUV = TexRegion{64, 0, 127, 63};
    const auto doorUV = TexRegion{0, 64, 63, 127};

    // draw floor
    VECTOR posZero{};
    TransMatrix(&worldmat, &posZero);
    for (int x = -3; x < 3; ++x) {
        for (int y = 0; y < 8; ++y) {
            floorTileObj.position.vx = x * tileSize * 2;
            floorTileObj.position.vz = -tileSize - y * tileSize * 2;
            drawTile(floorTileObj, floorMeshIdx, floorTextureIdx, tileUV);
        }
    }

    // draw walls
    // far
    for (int x = -3; x < 3; ++x) {
        wallTileObj.position.vx = x * tileSize * 2;
        wallTileObj.position.vy = -tileSize;
        wallTileObj.position.vz = 0;
        if (x == 0) {
            drawTile(wallTileObj, wallMeshIdx, bricksTextureIdx, doorUV);
        } else {
            drawTile(wallTileObj, wallMeshIdx, bricksTextureIdx, windowUV);
        }
    }

    // left
    for (int x = 0; x < 8; ++x) {
        wallTileObj.position.vx = -3 * tileSize * 2 - tileSize;
        wallTileObj.position.vy = -tileSize;
        wallTileObj.position.vz = -x * tileSize * 2;
        drawTile(wallTileObj, wallMeshLIdx, bricksTextureIdx, (x % 2 == 0) ? tileUV : windowUV);
    }

    // right
    for (int x = 0; x < 8; ++x) {
        wallTileObj.position.vx = 3 * tileSize * 2 - tileSize;
        wallTileObj.position.vy = -tileSize;
        wallTileObj.position.vz = -x * tileSize * 2;
        drawTile(wallTileObj, wallMeshRIdx, bricksTextureIdx, (x % 2 == 0) ? tileUV : windowUV);
    }
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

std::uint16_t Game::addMesh(Mesh mesh)
{
    auto id = meshes.size();
    meshes.push_back(std::move(mesh));
    return static_cast<std::uint16_t>(id);
}

std::uint16_t Game::addTexture(TIM_IMAGE texture)
{
    auto id = textures.size();
    textures.push_back(std::move(texture));
    return static_cast<std::uint16_t>(id);
}
