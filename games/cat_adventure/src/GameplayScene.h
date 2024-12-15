#pragma once

#include <psyqo/scene.hh>

#include "Armature.h"
#include "Camera.h"
#include "DialogueBox.h"
#include "FPSCounter.h"
#include "Object.h"
#include "Quaternion.h"
#include "SkeletalAnimation.h"
#include "SkeletonAnimator.h"
#include "StringHash.h"

class Game;
class Renderer;
class PadManager;

class GameplayScene : public psyqo::Scene {
public:
    GameplayScene(Game& game);

private:
    void start(StartReason reason) override;

    void frame() override;

    void processInput(const PadManager& pad);
    void processPlayerInput(const PadManager& pad);
    void processFreeCameraInput(const PadManager& pad);
    void processDebugInput(const PadManager& pad);
    void update();
    void updateCamera();

    void draw(Renderer& renderer);
    void drawTestLevel(Renderer& renderer);
    void drawDebugInfo(Renderer& renderer);

    Game& game;

    // game objects
    ModelObject car;
    ModelObject levelObj;

    ModelObject player;
    SkeletonAnimator playerAnimator;

    Camera camera;

    DialogueBox dialogueBox;

    std::size_t animIndex{1};

    FPSCounter fpsCounter;

    bool debugInfoDrawn{true};
    bool freeCamera{false};
};
