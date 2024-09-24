#include "Game.h"

#include <stdio.h>
#include <psyqo/alloc.h>
#include <psyqo/gte-registers.hh>
#include <libetc.h>
#include <libcd.h>

#include <utility> //std::move

#include "Utils.h"
#include "Globals.h"

#include "Trig.h"

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

void Game::draw()
{
    ClearOTagR(ot[currBuffer], OTLEN);
    nextpri = primbuff[currBuffer];

    renderer::RenderCtx ctx{
        .ot = ot[currBuffer],
        .OTLEN = OTLEN,
        .nextpri = nextpri,
        .currBuffer = currBuffer,
        .screenClip = screenClip,
        .worldmat = worldmat,
        .viewmat = viewmat,
        .camera = camera,
    };

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
    ctx.nextpri = renderer::drawModelFast(ctx, roll, rollModelInstances[0]);

    auto& bricksTexture = textures[bricksTextureIdx];
    ctx.nextpri = renderer::drawModel(ctx, level, levelModel, bricksTexture, true);

    for (int i = 1; i < numRolls; ++i) {
        roll.position.vx += 128;
        ctx.nextpri = renderer::drawModelFast(ctx, roll, rollModelInstances[i]);
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
