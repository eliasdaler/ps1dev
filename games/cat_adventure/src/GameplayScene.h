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

class GameplayScene : public psyqo::Scene {
public:
    GameplayScene(Game& game);
    void onResourcesLoaded();

private:
    void start(StartReason reason) override;

    void frame() override;

    void processInput();
    void processDebugInput();
    void update();
    void updateCamera();

    void draw(Renderer& renderer);
    void drawTestLevel(Renderer& renderer);
    void drawDebugInfo(Renderer& renderer);

    // game objects
    ModelObject car;
    ModelObject levelObj;

    ModelObject cato;
    SkeletonAnimator catoAnimator;

    Camera camera;

    Game& game;

    DialogueBox dialogueBox;

    int toneNum{0};
    int reverbPreset{0};

    std::size_t animIndex{1};

    FPSCounter fpsCounter;
};
