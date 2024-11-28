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

    int findChannel(int trackId, int noteId);
    void freeChannel(int trackId, int nodeId);

    void frame() override;

    void findBPM();
    void updateMusic();

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

    eastl::array<int, 16> lastEvent{};
    eastl::array<int, 16> eventNum{};
    eastl::array<int, 16> currentInst{};

    eastl::array<bool, 24> usedChannels;
    struct UseInfo {
        int trackId;
        int noteId;
    };
    eastl::array<UseInfo, 24> channelUsers;

    unsigned musicTimer;
    int musicTime{0};

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

    int32_t chanMaskOn{0};
    int32_t chanMaskOff{0};

    uint32_t bpm{102};
    uint32_t microsecondsPerClick{0};
    uint32_t ticksPerClick{0};
    uint32_t waitHBlanks{0};
};
