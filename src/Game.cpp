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

    renderer.init();

    setVector(&renderer.camera.position, 0, -ONE * (tileSize + tileSize / 5), -ONE * 2500);

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
    auto& camera = renderer.camera;

    camera.trot.vx = camera.rotation.vx >> 12;
    camera.trot.vy = camera.rotation.vy >> 12;
    camera.trot.vz = camera.rotation.vz >> 12;

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
        camera.position.vx -= math::icos(camera.trot.vy) << 4;
        camera.position.vz -= math::isin(camera.trot.vy) << 4;
    }
    if (PadStatus & PADR1) {
        camera.position.vx += math::icos(camera.trot.vy) << 4;
        camera.position.vz += math::isin(camera.trot.vy) << 4;
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
        camera.position.vx -= math::isin(camera.trot.vy) << 5;
        camera.position.vz += math::icos(camera.trot.vy) << 5;
    }
    if (PadStatus & PADLdown) {
        camera.position.vx += math::isin(camera.trot.vy) << 5;
        camera.position.vz -= math::icos(camera.trot.vy) << 5;
    }
}

void Game::update()
{}

std::uint16_t Game::addTexture(TIM_IMAGE texture)
{
    auto id = textures.size();
    textures.push_back(std::move(texture));
    return static_cast<std::uint16_t>(id);
}

void Game::draw()
{
    renderer.beginDraw();

    auto& bricksTexture = textures[bricksTextureIdx];
    renderer.drawModel(level, levelModel, bricksTexture, true);

    auto pos = roll.position;
    renderer.drawModelFast(roll, rollModelInstances[0]);
    for (int i = 1; i < numRolls; ++i) {
        roll.position.vx += 128;
        renderer.drawModelFast(roll, rollModelInstances[i]);
    }

    roll.position = pos;

    FntPrint(
        "X=%d Y=%d Z=%d\n",
        renderer.camera.position.vx >> 12,
        renderer.camera.position.vy >> 12,
        renderer.camera.position.vz >> 12);
    FntPrint("RX=%d, RY=%d\n", renderer.camera.trot.vx, renderer.camera.trot.vy);

    FntFlush(-1);

    renderer.display();
}
