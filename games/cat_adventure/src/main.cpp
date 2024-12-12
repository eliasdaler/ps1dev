#include "Game.h"
#include "GameplayScene.h"
#include "LoadingScene.h"
#include "Renderer.h"

#include "matrix_test.h"

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

Game game;

// REMOVE THIS!!!
GameplayScene gameplayScene{game};
LoadingScene loadingScene{game};
} // namespace

psyqo::Coroutine<> loadCoroutine(Game* game)
{
    game->levelId = startLevel;

    psyqo::Coroutine<>::Awaiter awaiter = game->cdLoadCoroutine.awaiter();

    game->loadModel(getLevelModelPath(game->levelId), game->levelModel);
    co_await awaiter;

    game->loadModel("CATO.BIN;1", game->catoModel);
    co_await awaiter;

    game->loadModel("CAR.BIN;1", game->carModel);
    co_await awaiter;

    game->loadTIM("BRICKS.TIM;1", game->bricksTexture);
    co_await awaiter;

    game->loadTIM("CATO.TIM;1", game->catoTexture);
    co_await awaiter;

    game->loadTIM("CAR.TIM;1", game->carTexture);
    co_await awaiter;

    game->loadTIM("FONT.TIM;1", game->fontTexture);
    co_await awaiter;

    game->loadFont("FONT.FNT;1", game->font);
    co_await awaiter;

    game->loadAnimations("HUMAN.ANM;1", game->animations);
    co_await awaiter;

    game->loadMIDI("SONG.MID;1", game->midi);
    co_await awaiter;

    game->loadInstruments("INST.VAB;1", game->vab);
    co_await awaiter;

    game->loadRawPCM("SMPL.PCM;1", 0x1010);
    co_await awaiter;

    game->step1Sound = 0x3300;
    game->loadSound("STEP1.VAG;1", game->step1Sound);
    co_await awaiter;

    game->step2Sound = 0x3F00;
    game->loadSound("STEP2.VAG;1", game->step2Sound);
    co_await awaiter;

    game->popScene();

    game->pushScene(&gameplayScene);
}

void Game::createScene()
{
    // testing::testMatrix();
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    pad.init();

    pushScene(&loadingScene);

    // load resources
    cdLoadCoroutine = loadCoroutine(this);
    cdLoadCoroutine.resume();
}

int main()
{
    return game.run();
}
