#include "Game.h"
#include "GameplayScene.h"
#include "LoadingScene.h"
#include "Renderer.h"

#include "matrix_test.h"

namespace
{
Game game;
Renderer renderer;

GameplayScene gameplayScene{game, renderer};
LoadingScene loadingScene{game, renderer};
} // namespace

psyqo::Coroutine<> loadCoroutine(Game* game)
{
    psyqo::Coroutine<>::Awaiter awaiter = game->cdLoadCoroutine.awaiter();

    game->loadModel("LEVEL.BIN;1", game->levelModel);
    co_await awaiter;

    game->loadModel("CATO.BIN;1", game->catoModel);
    co_await awaiter;

    game->loadTIM("BRICKS.TIM;1", game->bricksTexture);
    co_await awaiter;

    game->loadTIM("CATO.TIM;1", game->catoTexture);
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
