#include "LoadingScene.h"

#include "Game.h"
#include "Renderer.h"

LoadingScene::LoadingScene(Game& game, Renderer& renderer) : game(game), renderer(renderer)
{}

void LoadingScene::start(StartReason reason)
{}

void LoadingScene::frame()
{
    draw();
}

void LoadingScene::draw()
{
    const auto parity = gpu().getParity();
    auto& ot = renderer.ots[parity];

    auto& primBuffer = renderer.primBuffers[parity];
    primBuffer.reset();
    // fill bg
    psyqo::Color bg{{.r = 0, .g = 0, .b = 0}};
    auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
    gpu().getNextClear(fill.primitive, bg);
    gpu().chain(fill);
    // text
    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
    game.romFont.chainprintf(game.gpu(), {{.x = 16, .y = 32}}, textCol, "Loading...");
}
