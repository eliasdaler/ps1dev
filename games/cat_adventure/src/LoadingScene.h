#pragma once

#include <psyqo/scene.hh>

class Game;
class Renderer;

class LoadingScene : public psyqo::Scene {
public:
    LoadingScene(Game& game);

private:
    void start(StartReason reason) override;

    void frame() override;

    void draw(Renderer& renderer);

    Game& game;
};
