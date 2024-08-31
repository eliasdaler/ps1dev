#include "Game.h"

#include <libetc.h>
#include <libcd.h>

#include <utility> // move

#include "Utils.h"

#include "inline_n.h"

#include "Trig.h"

namespace
{

constexpr auto tileSize = 32;

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

void loadModel(Mesh& mesh, eastl::string_view filename)
{
    const auto data = util::readFile(filename);
    const auto* bytes = (u_char*)data.data();

    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numverts = fr.GetInt16BE();
    mesh.vertices.resize(numverts);
    for (int i = 0; i < numverts; i++) {
        mesh.vertices[i].vx = fr.GetInt16BE();
        mesh.vertices[i].vy = fr.GetInt16BE();
        mesh.vertices[i].vz = fr.GetInt16BE();
    }

    // faces
    const auto numfaces = fr.GetInt16BE() * 4;
    mesh.faces.resize(numfaces);
    for (int i = 0; i < numfaces; i++) {
        mesh.faces[i] = fr.GetInt16BE();
    }

    // colors
    const auto numcolors = fr.GetInt8();
    mesh.colors.resize(numcolors);
    for (int i = 0; i < numcolors; i++) {
        mesh.colors[i] = fr.GetObj<CVECTOR>();
    }
}

}

void Game::init()
{
    PadInit(0);
    ResetGraph(0);

    InitGeom();
    SetGeomOffset(CENTERX, CENTERY);
    SetGeomScreen(CENTERX);

    SetDefDispEnv(&dispEnv[0], 0, 0, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&drawEnv[0], 0, SCREENYRES, SCREENXRES, SCREENYRES);

    SetDefDispEnv(&dispEnv[1], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&drawEnv[1], 0, 0, SCREENXRES, SCREENYRES);

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

    setVector(&camera.position, 0, -100, -100);
    setVector(&camera.position, 0, -ONE * 24, -100);
    camera.view = (MATRIX){0};

    CdInit();

    const auto textureData2 = util::readFile("\\BRICKS.TIM;1");
    bricksTextureIdx = addTexture(loadTexture(textureData2));

    const auto textureData = util::readFile("\\FLOOR.TIM;1");
    floorTextureIdx = addTexture(loadTexture(textureData));

    const auto textureData3 = util::readFile("\\FTEX.TIM;1");
    fTextureIdx = addTexture(loadTexture(textureData3));

    Mesh cubeMesh;
    loadModel(cubeMesh, "\\MODEL.BIN;1");
    cubeMeshIdx = addMesh(std::move(cubeMesh));

    cube.position = {-64, -32, 0};
    cube.rotation = {};
    cube.scale = {ONE >> 4, ONE >> 4, ONE >> 4};

    Mesh floorTile;
    floorTile.vertices = {
        SVECTOR{-tileSize, 0, tileSize},
        SVECTOR{tileSize, 0, tileSize},
        SVECTOR{-tileSize, 0, -tileSize},
        SVECTOR{tileSize, 0, -tileSize},
    };
    floorTile.faces = {0, 1, 2, 3};
    floorMeshIdx = addMesh(std::move(floorTile));

    floorTileObj.position = {0, 0, 0};
    floorTileObj.rotation = {};
    floorTileObj.scale = {ONE, ONE, ONE};

    Mesh wallTileMesh;
    wallTileMesh.vertices = {
        SVECTOR{-tileSize, -tileSize, 0},
        SVECTOR{tileSize, -tileSize, 0},
        SVECTOR{-tileSize, tileSize, 0},
        SVECTOR{tileSize, tileSize, 0},
    };
    wallTileMesh.faces = {0, 1, 2, 3};
    wallMeshIdx = addMesh(std::move(wallTileMesh));

    Mesh wallTileMeshL;
    wallTileMeshL.vertices = {
        SVECTOR{0, -tileSize, -tileSize},
        SVECTOR{0, -tileSize, tileSize},
        SVECTOR{0, tileSize, -tileSize},
        SVECTOR{0, tileSize, tileSize},
    };
    wallTileMeshL.faces = {0, 1, 2, 3};
    wallMeshLIdx = addMesh(std::move(wallTileMeshL));

    Mesh wallTileMeshR;
    wallTileMeshR.vertices = {
        SVECTOR{0, -tileSize, tileSize},
        SVECTOR{0, -tileSize, -tileSize},
        SVECTOR{0, tileSize, tileSize},
        SVECTOR{0, tileSize, -tileSize},
    };
    wallTileMeshR.faces = {0, 1, 2, 3};
    wallMeshRIdx = addMesh(std::move(wallTileMeshR));

    wallTileObj.position = {0, 0, 0};
    wallTileObj.rotation = {};
    wallTileObj.scale = {ONE, ONE, ONE};

    tileMesh.vertices.resize(4);
    tileMesh.faces = {0, 1, 2, 3};
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
#if 0
    if (PadStatus & PADLup) cube.position.vz += 2;
    if (PadStatus & PADLdown) cube.position.vz -= 2;

    if (PadStatus & PADLleft) cube.position.vx -= 2;
    if (PadStatus & PADLright) cube.position.vx += 2;
#else

    if (PadStatus & PADLleft) camera.rotation.vy += ONE * 10;
    if (PadStatus & PADLright) camera.rotation.vy -= ONE * 10;

    if (PadStatus & PADLup) {
        camera.position.vx -= math::isin(trot.vy) << 2;
        camera.position.vz += math::icos(trot.vy) << 2;
    }

    if (PadStatus & PADLdown) {
        camera.position.vx += math::isin(trot.vy) << 2;
        camera.position.vz -= math::icos(trot.vy) << 2;
    }

#endif
}

void Game::update()
{}

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

    auto tileUV = TexRegion{0, 0, 63, 63};
    auto windowUV = TexRegion{64, 0, 127, 63};
    auto doorUV = TexRegion{0, 64, 63, 127};

    // draw floor
    VECTOR posZero{};
    TransMatrix(&worldmat, &posZero);
    for (int x = -8; x < 8; ++x) {
        for (int y = -8; y < 8; ++y) {
            floorTileObj.position.vx = x * tileSize * 2;
            floorTileObj.position.vz = y * tileSize * 2;
            drawObject(floorTileObj, floorMeshIdx, floorTextureIdx, tileUV, true, &tileMesh);
        }
    }

    // draw walls
    for (int x = -3; x < 3; ++x) {
        wallTileObj.position.vx = x * tileSize * 2;
        wallTileObj.position.vy = -32;
        wallTileObj.position.vz = 128 + 64;
        if (x == 0) {
            drawObject(wallTileObj, wallMeshIdx, bricksTextureIdx, doorUV, true, &tileMesh);
        } else {
            drawObject(wallTileObj, wallMeshIdx, bricksTextureIdx, windowUV, true, &tileMesh);
        }
    }

    for (int x = 0; x < 8; ++x) {
        wallTileObj.position.vx = -3 * tileSize * 2 - 32;
        wallTileObj.position.vy = -32;
        wallTileObj.position.vz = 128 + 32 - x * tileSize * 2;
        drawObject(
            wallTileObj,
            wallMeshLIdx,
            bricksTextureIdx,
            (x % 2 == 0) ? tileUV : windowUV,
            true,
            &tileMesh);
    }

    for (int x = 0; x < 8; ++x) {
        wallTileObj.position.vx = 3 * tileSize * 2 - 32;
        wallTileObj.position.vy = -32;
        wallTileObj.position.vz = 128 + 32 - x * tileSize * 2;
        drawObject(
            wallTileObj,
            wallMeshRIdx,
            bricksTextureIdx,
            (x % 2 == 0) ? tileUV : windowUV,
            true,
            &tileMesh);
    }

    // draw player
    drawObject(cube, cubeMeshIdx, fTextureIdx, tileUV);

    FntPrint(
        "X=%d Y=%d Z=%d\n",
        camera.position.vx >> 12,
        camera.position.vy >> 12,
        camera.position.vz >> 12);
    FntPrint("RY=%d\n", trot.vy);

    FntFlush(-1);

    display();
}

void Game::drawObject(
    Object& object,
    std::uint16_t meshIdx,
    std::uint16_t textureIdx,
    const TexRegion& uvs,
    bool cpuTrans,
    Mesh* cpuMesh)
{
    // Draw the object
    RotMatrix(&object.rotation, &worldmat);
    if (!cpuTrans) {
        TransMatrix(&worldmat, &object.position);
    }
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.view, &worldmat, &viewmat);

    SetRotMatrix(&viewmat);
    SetTransMatrix(&viewmat);

    auto& mesh = cpuTrans ? *cpuMesh : meshes[meshIdx];
    if (cpuTrans) {
        auto& protoMesh = meshes[meshIdx];
        for (int i = 0; i < protoMesh.vertices.size(); ++i) {
            mesh.vertices[i].vx = protoMesh.vertices[i].vx + object.position.vx;
            mesh.vertices[i].vy = protoMesh.vertices[i].vy + object.position.vy;
            mesh.vertices[i].vz = protoMesh.vertices[i].vz + object.position.vz;
        }
    }

    auto& texture = textures[textureIdx];

    for (int i = 0, q = 0; i < mesh.faces.size(); i += 4, q++) {
        auto* polyft4 = (POLY_FT4*)nextpri;
        setPolyFT4(polyft4);

        setRGB0(polyft4, 128, 128, 128);
        setUV4(polyft4, uvs.u0, uvs.v0, uvs.u1, uvs.v0, uvs.u0, uvs.v1, uvs.u1, uvs.v1);

        polyft4->tpage = getTPage(texture.mode & 0x3, 0, texture.prect->x, texture.prect->y);
        polyft4->clut = getClut(texture.crect->x, texture.crect->y);

        /* gte_ldv0(&object.vertices[object.faces[i + 0]]);
        gte_ldv1(&object.vertices[object.faces[i + 1]]);
        gte_ldv2(&object.vertices[object.faces[i + 2]]);

        gte_rtpt();

        gte_nclip();

        long nclip;
        gte_stopz(&nclip);

        if (nclip < 0) {
            continue;
        }

        gte_stsxy0(&polyft4->x0);

        gte_ldv0(&object.vertices[object.faces[i + 3]]);
        gte_rtps();

        gte_stsxy3(&polyft4->x1, &polyft4->x2, &polyft4->x3);

        gte_avsz4();
        gte_stotz(&otz); */

        long p, otz, flg;

        const auto nclip = RotAverageNclip4(
            &mesh.vertices[mesh.faces[i + 0]],
            &mesh.vertices[mesh.faces[i + 1]],
            &mesh.vertices[mesh.faces[i + 2]],
            &mesh.vertices[mesh.faces[i + 3]],
            (long*)&polyft4->x0,
            (long*)&polyft4->x1,
            (long*)&polyft4->x2,
            (long*)&polyft4->x3,
            &p,
            &otz,
            &flg);

        if (nclip <= 0) {
            continue;
        }

        if (otz > 0 && otz < OTLEN) {
            addPrim(&ot[currBuffer][otz], polyft4);
            nextpri += sizeof(POLY_FT4);
        }
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
