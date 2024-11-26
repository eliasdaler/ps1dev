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
Renderer renderer{game.gpu()};

GameplayScene gameplayScene{game, renderer};
LoadingScene loadingScene{game, renderer};
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

    game->loadSound("STEP1.VAG;1", game->stepSound);
    co_await awaiter;

    game->loadSound("DRUM.VAG;1", game->drumSound);
    co_await awaiter;

    game->loadSound("SYNTH.VAG;1", game->synthSound);
    co_await awaiter;

    game->loadSound("GUITAR.VAG;1", game->guitarSound);
    co_await awaiter;

    game->loadSound("MELODY.VAG;1", game->melodySound);
    co_await awaiter;

    game->loadSound("MELODY2.VAG;1", game->melody2Sound);
    co_await awaiter;

    game->soundPlayer.uploadSound(0x1010, game->stepSound);
    game->soundPlayer.uploadSound(0x4040, game->synthSound);
    game->soundPlayer.uploadSound(0x8080, game->drumSound);
    game->soundPlayer.uploadSound(0xC0C0, game->guitarSound);
    game->soundPlayer.uploadSound(0xE0E0, game->melodySound);
    game->soundPlayer.uploadSound(0xFFA0, game->melody2Sound);

    game->loadMIDI("SONG.MID;1", game->midi);
    co_await awaiter;

    game->loadInstruments("INST.VAB;1", game->vab);
    co_await awaiter;

    game->popScene();

    gameplayScene.onResourcesLoaded();
    game->pushScene(&gameplayScene);
}

void Game::createScene()
{
    testing::testMatrix();
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    pad.initialize();

    pushScene(&loadingScene);

    // load resources
    cdLoadCoroutine = loadCoroutine(this);
    cdLoadCoroutine.resume();
}

int main()
{
    return game.run();
}
