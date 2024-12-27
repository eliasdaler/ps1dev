#include "Game.h"

#include <common/syscalls/syscalls.h>

#include "StringHashes.h"

namespace
{
const int startLevel = 0;

const char* getLevelModelPath(int levelId)
{
    switch (levelId) {
    case 0:
        return "LEVEL.BIN;1";
    case 1:
        return "LEVEL2.BIN;1";
    }
    return "";
}
}

Game::Game() :
    cd(*this),
    renderer(gpu()),
    songPlayer(gpu(), soundPlayer),
    gameplayScene(*this),
    loadingScene(*this)
{}

void Game::prepare()
{
    initStringHashes();

    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::NTSC)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);

    cd.init();

    soundPlayer.init();
    renderer.init();
}

namespace
{
psyqo::Coroutine<> loadCoroutine(Game& game)
{
    game.levelId = startLevel;

    psyqo::Coroutine<>::Awaiter awaiter = game.gameLoadCoroutine.awaiter();

    { // core
        game.cd.loadTIM("FONT.TIM;1", game.fontTexture);
        co_await awaiter;

        game.cd.loadFont("FONT.FNT;1", game.font);
        co_await awaiter;

        game.debugMenu.init(game.font, game.fontTexture, game.fontTexture);
    }

    game.cd.loadModel(getLevelModelPath(game.levelId), game.levelModel);
    co_await awaiter;

    game.cd.loadModel(getLevelModelPath(1), game.level2Model);
    co_await awaiter;

    game.cd.loadModel("CATO.BIN;1", game.catoModel);
    co_await awaiter;

    game.cd.loadModel("HUMAN.BIN;1", game.humanModel);
    co_await awaiter;

    game.cd.loadModel("CAR.BIN;1", game.carModel);
    co_await awaiter;

    game.cd.loadTIM("BRICKS.TIM;1", game.bricksTexture);
    co_await awaiter;

    game.cd.loadTIM("CATO.TIM;1", game.catoTexture);
    co_await awaiter;

    game.cd.loadTIM("CAR.TIM;1", game.carTexture);
    co_await awaiter;

    game.cd.loadAnimations("HUMAN.ANM;1", game.animations);
    co_await awaiter;

    game.cd.loadMIDI("SONG.MID;1", game.midi);
    co_await awaiter;

    game.cd.loadInstruments("INST.VAB;1", game.vab);
    co_await awaiter;

    game.cd.loadRawPCM("SMPL.PCM;1", 0x1010);
    co_await awaiter;

    game.step1Sound = 0x3300;
    game.cd.loadSound("STEP1.VAG;1", game.step1Sound);
    co_await awaiter;

    game.step2Sound = 0x3F00;
    game.cd.loadSound("STEP2.VAG;1", game.step2Sound);
    co_await awaiter;

    game.popScene(); // pop loading scene
    game.pushScene(&game.gameplayScene);
}
}

void Game::createScene()
{
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    pad.init();

    pushScene(&loadingScene);

    // load resources
    gameLoadCoroutine = loadCoroutine(*this);
    gameLoadCoroutine.resume();
}
