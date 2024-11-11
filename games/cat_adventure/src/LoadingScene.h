#pragma once

#include <psyqo/scene.hh>

class Game;
class Renderer;

class LoadingScene : public psyqo::Scene {
public:
    LoadingScene(Game& game, Renderer& renderer);

private:
    void start(StartReason reason) override;

    void frame() override;

    void draw();

    Game& game;
    Renderer& renderer;
};
