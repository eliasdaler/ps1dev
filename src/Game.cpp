#include "Game.h"

#include <libetc.h>
#include <stdio.h>

#include "cube.h"

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

    printf("Hello from printf!\n");

    int* x = new int(42);
    printf("Test int: %d\n", *x);
    delete x;

    bool palMode = false;
    if (palMode) {
        SetVideoMode(MODE_PAL);
        dispEnv[0].screen.y += 8;
        dispEnv[1].screen.y += 8;
    }

    SetDispMask(1); // Display on screen

    setRGB0(&drawEnv[0], 255, 149, 237);
    setRGB0(&drawEnv[1], 255, 149, 237);
    drawEnv[0].isbg = 1;
    drawEnv[1].isbg = 1;

    PutDispEnv(&dispEnv[currBuffer]);
    PutDrawEnv(&drawEnv[currBuffer]);

    FntLoad(960, 0);
    FntOpen(16, 16, 196, 64, 0, 256);

    rotation = SVECTOR{232, 232, 0, 0};
    translation = VECTOR{0, 0, CENTERX * 3, 0};
    scale = VECTOR{ONE / 2, ONE / 2, ONE / 2, 0};

    matrix = {};
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
        if (PadStatus & PADL1) translation.vz -= 4;
        if (PadStatus & PADR1) translation.vz += 4;
        if (PadStatus & PADL2) rotation.vz -= 8;
        if (PadStatus & PADR2) rotation.vz += 8;
        if (PadStatus & PADLup) rotation.vx -= 8;
        if (PadStatus & PADLdown) rotation.vx += 8;
        if (PadStatus & PADLleft) rotation.vy -= 8;
        if (PadStatus & PADLright) rotation.vy += 8;
        if (PadStatus & PADRup) translation.vy -= 2;
        if (PadStatus & PADRdown) translation.vy += 2;
        if (PadStatus & PADRleft) translation.vx -= 2;
        if (PadStatus & PADRright) translation.vx += 2;
        if (PadStatus & PADselect) {
            rotation.vx = rotation.vy = rotation.vz = 0;
            scale.vx = scale.vy = scale.vz = ONE / 2;
            translation.vx = translation.vy = 0;
            translation.vz = CENTERX * 3;
        }
    }

    if (PadStatus & PADstart) {
        if (!startPressed) {
            autoRotate = (autoRotate + 1) & 1;
            rotation.vx = rotation.vy = rotation.vz = 0;
            scale.vx = scale.vy = scale.vz = ONE / 2;
            translation.vx = translation.vy = 0;
            translation.vz = CENTERX * 3;
        }
        startPressed = true;
    } else {
        startPressed = false;
    }
}

void Game::update()
{
    if (autoRotate) {
        rotation.vy += 28;
        rotation.vx += 28;
    }
}

void Game::draw()
{
    ClearOTagR(ot[currBuffer], OTLEN);
    nextpri = primbuff[currBuffer];

    drawCube(modelCube);

    FntPrint("Hello world!");
    FntFlush(-1);

    display();
}

void Game::drawCube(const TMESH& cube)
{
    RotMatrix(&rotation, &matrix);
    TransMatrix(&matrix, &translation);
    ScaleMatrix(&matrix, &scale);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);

    for (int i = 0; i < cube.len * 3; i += 3) {
        const auto polyg3 = (POLY_G3*)nextpri;
        setPolyG3(polyg3);

        const auto c0 = cube.c[i];
        const auto c1 = cube.c[i + 2];
        const auto c2 = cube.c[i + 1];
        setRGB0(polyg3, c0.r, c0.g, c0.b);
        setRGB1(polyg3, c1.r, c1.g, c1.b);
        setRGB2(polyg3, c2.r, c2.g, c2.b);

        long p, otz, flags;
        const auto nclip = RotAverageNclip3(
            &modelCube_mesh[modelCube_index[i]],
            &modelCube_mesh[modelCube_index[i + 2]],
            &modelCube_mesh[modelCube_index[i + 1]],
            (long*)&polyg3->x0,
            (long*)&polyg3->x1,
            (long*)&polyg3->x2,
            &p,
            &otz,
            &flags);
        if (nclip <= 0) {
            continue;
        }
        if ((otz > 0) && (otz < OTLEN)) {
            addPrim(&ot[currBuffer][otz], polyg3);
        }
        nextpri += sizeof(POLY_G3);
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
