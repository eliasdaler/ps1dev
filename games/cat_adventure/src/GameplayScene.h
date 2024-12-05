#pragma once

#include <psyqo/scene.hh>

#include "Camera.h"
#include "DialogueBox.h"
#include "Object.h"
#include "Quaternion.h"
#include "Skeleton.h"

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

    int frameDiff{0};
    int lastFrameCounter{0};

    psyqo::FixedPoint<> alpha{0.8};
    psyqo::FixedPoint<> oneMinAlpha{1.0 - 0.8};
    psyqo::FixedPoint<> fpsMovingAverageOld{};
    psyqo::FixedPoint<> fpsMovingAverageNew{};

    psyqo::FixedPoint<> newFPS{0.f};
    psyqo::FixedPoint<> avgFPS{0.f};
    psyqo::FixedPoint<> lerpFactor{0.1};

    int toneNum{0};
    int reverbPreset{0};

    Quaternion startRotation;
    Quaternion targetRotation;
    psyqo::FixedPoint<12, std::int16_t> slerpFactor{};
};
