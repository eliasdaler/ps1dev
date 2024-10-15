#include "Game.h"

#include <stdio.h>
#include <libetc.h>
#include <libcd.h>

#include <utility>

#include "Utils.h"
#include "Trig.h"

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

}

void Game::init()
{
    util::clearAllGTERegisters();

    PadInit(0);

    renderer.init();

    auto& camera = renderer.camera;
    setVector(&camera.position, 0, -ONE * 1507, -ONE * 2500);

    // testing
    camera.position.vx = ONE * 3588;
    camera.position.vy = ONE * -1507;
    camera.position.vz = ONE * -10259;
    camera.rotation.vx = ONE * 150;
    camera.rotation.vy = ONE * 300;

    camera.position.vx = 0;
    camera.position.vy = 0;
    camera.position.vz = 0;
    camera.rotation.vx = 0;
    camera.rotation.vy = 0;


    CdInit();
    soundPlayer.init();

    const auto textureData2 = util::readFile("\\BRICKS.TIM;1");
    bricksTextureIdx = addTexture(loadTexture(textureData2));

    printf("Load models...\n");

    Sound sound;
    sound.load("\\STEP1.VAG;1");
    soundPlayer.transferVAGToSpu(sound, SPU_0CH);
    // soundPlayer.playAudio(SPU_0CH);

    levelModel.load("\\LEVEL.BIN;1");
    humanModel.load("\\HUMAN.BIN;1");

    level.position = {0, 0, 0};
    level.rotation = {};
    level.scale = {ONE, ONE, ONE};
    level.model = &levelModel;

    human.position = {0, ONE, 0};
    human.rotation = {};
    human.scale = {ONE, ONE, ONE};
    human.model = &humanModel;

    const auto textureData4 = util::readFile("\\CATO.TIM;1");
    catoTextureIdx = addTexture(loadTexture(textureData4));
    catoModel.load("\\CATO.BIN;1");
    cato.position = {-512, ONE, 0};
    cato.rotation = {};
    cato.scale = {ONE, ONE, ONE};
    cato.model = &catoModel;

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

    auto fac = 2;
    auto strafeSpeed = 5;
    auto walkSpeed = 6;

    // rotating the camera around Y
    if (PadStatus & PADLleft) {
        camera.rotation.vy += ONE * 25 * fac;
    }
    if (PadStatus & PADLright) {
        camera.rotation.vy -= ONE * 25 * fac;
    }

    // strafing
    if (PadStatus & PADL1) {
        camera.position.vx -= math::icos(camera.trot.vy) << strafeSpeed;
        camera.position.vz -= math::isin(camera.trot.vy) << strafeSpeed;
    }
    if (PadStatus & PADR1) {
        camera.position.vx += math::icos(camera.trot.vy) << strafeSpeed;
        camera.position.vz += math::isin(camera.trot.vy) << strafeSpeed;
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
        camera.position.vx -= math::isin(camera.trot.vy) << walkSpeed;
        camera.position.vz += math::icos(camera.trot.vy) << walkSpeed;
    }
    if (PadStatus & PADLdown) {
        camera.position.vx += math::isin(camera.trot.vy) << walkSpeed;
        camera.position.vz -= math::icos(camera.trot.vy) << walkSpeed;
    }
}

void Game::update()
{
    numTicks++;
    if (numTicks % 50 == 0) {
        soundPlayer.playAudio(SPU_0CH);
    }
}

std::uint16_t Game::addTexture(TIM_IMAGE texture)
{
    auto id = textures.size();
    textures.push_back(std::move(texture));
    return static_cast<std::uint16_t>(id);
}

void Game::draw()
{
    renderer.beginDraw();

#if 0
    auto& bricksTexture = textures[bricksTextureIdx];
    renderer.drawModel(level, *level.model, bricksTexture);

    auto& catoTexture = textures[catoTextureIdx];
    // renderer.drawModel(cato, *cato.model, catoTexture, 256);
    // renderer.drawModel(human, *human.model, bricksTexture, 256);
#endif

    FntPrint(
        "X=%d Y=%d Z=%d\n",
        renderer.camera.position.vx >> 12,
        renderer.camera.position.vy >> 12,
        renderer.camera.position.vz >> 12);
    FntPrint(
        "X=%d Y=%d Z=%d\n",
        renderer.camera.tpos.vx,
        renderer.camera.tpos.vy,
        renderer.camera.tpos.vz);
    FntPrint(
        "RX=%d, RY=%d\n, cos=%d, sin=%d",
        renderer.camera.trot.vx,
        renderer.camera.trot.vy,
        math::icos(230),
        math::isin(230));

    FntFlush(-1);

    renderer.display();
}
