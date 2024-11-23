#pragma once

#include <psyqo/scene.hh>

#include "Camera.h"
#include "DialogueBox.h"
#include "Object.h"

class Game;
class Renderer;

class GameplayScene : public psyqo::Scene {
public:
    GameplayScene(Game& game, Renderer& renderer);
    void onResourcesLoaded();

private:
    void start(StartReason reason) override;

    void frame() override;

    void processInput();
    void update();
    void updateCamera();

    void draw();
    void drawDebugInfo();

    // game objects
    ModelObject cato;
    ModelObject car;
    Camera camera;

    Game& game;
    Renderer& renderer;

    DialogueBox dialogueBox;

    int pitchBase;
    int reset{0};
    int count{100};
};
