#include "Game.h"

#include <libetc.h>
#include <libcd.h>

#include "Utils.h"

#include "inline_n.h"

namespace
{

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

    setRGB0(&drawEnv[0], 100, 149, 237);
    setRGB0(&drawEnv[1], 100, 149, 237);
    drawEnv[0].isbg = 1;
    drawEnv[1].isbg = 1;

    PutDispEnv(&dispEnv[currBuffer]);
    PutDrawEnv(&drawEnv[currBuffer]);

    FntLoad(960, 0);
    FntOpen(16, 16, 196, 64, 0, 256);

    setVector(&camera.position, 500, -1000, -1200);
    camera.lookat = (MATRIX){0};

    CdInit();

    const auto textureData = util::readFile("\\BRICKS.TIM;1");
    texture = loadTexture(textureData);

    loadModel(cube, "\\MODEL.BIN;1");

    cube.position = {};
    cube.rotation = {};
    cube.scale = {ONE * 2, ONE * 2, ONE * 2};
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
    if (!autoRotate) {
        if (PadStatus & PADL1) cube.position.vz -= 4;
        if (PadStatus & PADR1) cube.position.vz += 4;
        if (PadStatus & PADL2) cube.rotation.vz -= 8;
        if (PadStatus & PADR2) cube.rotation.vz += 8;
        if (PadStatus & PADLup) cube.rotation.vx -= 8;
        if (PadStatus & PADLdown) cube.rotation.vx += 8;
        if (PadStatus & PADLleft) cube.rotation.vy -= 8;
        if (PadStatus & PADLright) cube.rotation.vy += 8;
        if (PadStatus & PADRup) cube.position.vy -= 2;
        if (PadStatus & PADRdown) cube.position.vy += 2;
        if (PadStatus & PADRleft) cube.position.vx -= 2;
        if (PadStatus & PADRright) cube.position.vx += 2;
        if (PadStatus & PADselect) {
            cube.rotation = {};
            cube.position = {};
        }
    }

    if (PadStatus & PADstart) {
        if (!startPressed) {
            autoRotate = !autoRotate;
            cube.rotation = {};
            cube.position = {};
        }
        startPressed = true;
    } else {
        startPressed = false;
    }
}

void Game::update()
{
    if (autoRotate) {
        cube.rotation.vy += 28;
        cube.rotation.vx += 28;
    }
}

void Game::draw()
{
    ClearOTagR(ot[currBuffer], OTLEN);
    nextpri = primbuff[currBuffer];

    drawCube(cube);

    FntPrint("Hello world!");
    FntFlush(-1);

    display();
}

void Game::drawCube(Object& object)
{
    VECTOR globalUp{0, -ONE, 0};
    camera::lookAt(&camera, &camera.position, &object.position, &globalUp);

    // Draw the object
    RotMatrix(&object.rotation, &worldmat);
    TransMatrix(&worldmat, &object.position);
    ScaleMatrix(&worldmat, &object.scale);

    // Create the View Matrix combining the world matrix & lookat matrix
    CompMatrixLV(&camera.lookat, &worldmat, &viewmat);

    SetRotMatrix(&viewmat);
    SetTransMatrix(&viewmat);

    for (int i = 0, q = 0; i < cube.faces.size(); i += 4, q++) {
        auto* polyft4 = (POLY_FT4*)nextpri;
        setPolyFT4(polyft4);

        setRGB0(polyft4, 128, 128, 128);
        setUV4(polyft4, 0, 0, 63, 0, 0, 63, 63, 63);

        polyft4->tpage = getTPage(texture.mode & 0x3, 0, texture.prect->x, texture.prect->y);
        polyft4->clut = getClut(texture.crect->x, texture.crect->y);

        gte_ldv0(&object.vertices[object.faces[i + 0]]);
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
        long otz;
        gte_stotz(&otz);

        if ((otz > 0) && (otz < OTLEN)) {
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
