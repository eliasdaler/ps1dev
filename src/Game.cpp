#include "Game.h"

#include <libetc.h>
#include <libcd.h>

#include "Utils.h"

#include "inline_n.h"

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

void loadModel(Object& object, eastl::string_view filename)
{
    const auto data = util::readFile(filename);
    const auto* bytes = (u_char*)data.data();

    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numverts = fr.GetInt16BE();
    object.vertices.resize(numverts);
    for (int i = 0; i < numverts; i++) {
        object.vertices[i].vx = fr.GetInt16BE();
        object.vertices[i].vy = fr.GetInt16BE();
        object.vertices[i].vz = fr.GetInt16BE();
    }

    // faces
    const auto numfaces = fr.GetInt16BE() * 4;
    object.faces.resize(numfaces);
    for (int i = 0; i < numfaces; i++) {
        object.faces[i] = fr.GetInt16BE();
    }

    // colors
    const auto numcolors = fr.GetInt8();
    object.colors.resize(numcolors);
    for (int i = 0; i < numcolors; i++) {
        object.colors[i] = fr.GetObj<CVECTOR>();
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
    camera.lookat = (MATRIX){0};

    CdInit();

    const auto textureData2 = util::readFile("\\BRICKS.TIM;1");
    bricksTexture = loadTexture(textureData2);

    const auto textureData = util::readFile("\\FLOOR.TIM;1");
    floorTexture = loadTexture(textureData);

    const auto textureData3 = util::readFile("\\FTEX.TIM;1");
    fTexture = loadTexture(textureData3);

    loadModel(cube, "\\MODEL.BIN;1");

    cube.position = {0, -32, 0};
    cube.rotation = {};
    cube.scale = {ONE >> 4, ONE >> 4, ONE >> 4};

    floorTile.vertices = {
        SVECTOR{-tileSize, 0, tileSize},
        SVECTOR{tileSize, 0, tileSize},
        SVECTOR{-tileSize, 0, -tileSize},
        SVECTOR{tileSize, 0, -tileSize},
    };
    floorTile.faces = {0, 1, 2, 3};

    floorTile.position = {0, 0, 0};
    floorTile.rotation = {};
    floorTile.scale = {ONE, ONE, ONE};

    floorTileTemplate = floorTile;

    wallTile.vertices = {
        SVECTOR{-tileSize, -tileSize, 0},
        SVECTOR{tileSize, -tileSize, 0},
        SVECTOR{-tileSize, tileSize, 0},
        SVECTOR{tileSize, tileSize, 0},
    };
    wallTile.faces = {0, 1, 2, 3};

    wallTile.position = {0, 0, 0};
    wallTile.rotation = {};
    wallTile.scale = {ONE, ONE, ONE};

    wallTileTemplate = wallTile;
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
    const auto PadStatus = PadRead(0);
    if (PadStatus & PADLup) cube.position.vz += 2;
    if (PadStatus & PADLdown) cube.position.vz -= 2;

    if (PadStatus & PADLleft) cube.position.vx -= 2;
    if (PadStatus & PADLright) cube.position.vx += 2;

    /* if (PadStatus & PADLleft) cube.rotation.vy -= 20;
    if (PadStatus & PADLright) cube.rotation.vy += 20; */
}

void Game::update()
{}

void Game::draw()
{
    ClearOTagR(ot[currBuffer], OTLEN);
    nextpri = primbuff[currBuffer];

    // camera.position.vx = cube.position.vx;
    // camera.position.vz = cube.position.vz - 32;

    VECTOR globalUp{0, -ONE, 0};
    camera::lookAt(&camera, &camera.position, &cube.position, &globalUp);

    // draw floor
    VECTOR posZero{};
    TransMatrix(&worldmat, &posZero);
    for (int x = -8; x < 8; ++x) {
        for (int y = -8; y < 8; ++y) {
            floorTile.position.vx = x * tileSize * 2;
            floorTile.position.vz = y * tileSize * 2;
            drawObject(floorTile, floorTexture, true, &floorTileTemplate);
        }
    }

    // draw walls
    for (int x = -8; x < 8; ++x) {
        wallTile.position.vx = x * tileSize * 2;
        wallTile.position.vy = -32;
        wallTile.position.vz = 256;
        drawObject(wallTile, bricksTexture, true, &wallTileTemplate);
    }

    // draw player
    drawObject(cube, fTexture);

    FntPrint("Ppos: %d, %d, %d\n", cube.position.vx, cube.position.vy, cube.position.vz);
    FntPrint("Rot: %d\n", cube.rotation.vy);
    FntPrint("Cpos: %d, %d, %d", camera.position.vx, camera.position.vy, camera.position.vz);
    FntFlush(-1);

    display();
}

void Game::drawObject(Object& object, TIM_IMAGE& texture, bool cpuTrans, Object* tmpl)
{
    // Draw the object
    RotMatrix(&object.rotation, &worldmat);
    if (!cpuTrans) {
        TransMatrix(&worldmat, &object.position);
    }
    ScaleMatrix(&worldmat, &object.scale);

    CompMatrixLV(&camera.lookat, &worldmat, &viewmat);

    SetRotMatrix(&viewmat);
    SetTransMatrix(&viewmat);

    if (cpuTrans) {
        for (int i = 0; i < object.vertices.size(); ++i) {
            object.vertices[i].vx = tmpl->vertices[i].vx + object.position.vx;
            object.vertices[i].vy = tmpl->vertices[i].vy + object.position.vy;
            object.vertices[i].vz = tmpl->vertices[i].vz + object.position.vz;
        }
    }

    for (int i = 0, q = 0; i < object.faces.size(); i += 4, q++) {
        auto* polyft4 = (POLY_FT4*)nextpri;
        setPolyFT4(polyft4);

        setRGB0(polyft4, 128, 128, 128);
        setUV4(polyft4, 0, 0, 63, 0, 0, 63, 63, 63);

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
            &object.vertices[object.faces[i + 0]],
            &object.vertices[object.faces[i + 1]],
            &object.vertices[object.faces[i + 2]],
            &object.vertices[object.faces[i + 3]],
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
