#pragma once

#include <psyqo/scene.hh>

#include <Camera.h>
#include <Core/StringHash.h>
#include <Graphics/SkeletalAnimation.h>
#include <Graphics/SkeletonAnimator.h>
#include <Math/Quaternion.h>
#include <Object.h>
#include <Trigger.h>
#include <UI/DialogueBox.h>
#include <Util/FPSCounter.h>

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
    void updateLevelSwitch();
    void handleCollision(psyqo::SoftMath::Axis axis);
    void updateCamera();

    void draw(Renderer& renderer);
    void drawTestLevel(Renderer& renderer);
    void drawDebugInfo(Renderer& renderer);
    void dumpDebugInfoToTTY();

    void switchLevel(int levelId);

    Game& game;

    // game objects
    ModelObject levelObj;

    AnimatedModelObject player;
    psyqo::Vec3 oldPlayerPos;
    AnimatedModelObject npc;

    Camera camera;

    TextureInfo uiTexture;

    DialogueBox dialogueBox;
    DialogueBox interactionDialogueBox;

    std::size_t animIndex{1};

    FPSCounter fpsCounter;

    bool debugInfoDrawn{true};
    bool collisionDrawn{false};
    bool freeCamera{false};
    bool followCamera{false};

    bool canTalk{false};
    bool canInteract{false};

    enum class GameState { Normal, Dialogue, SwitchLevel };
    GameState gameState;

    enum class SwitchLevelState { FadeOut, Delay, FadeIn, Done };
    SwitchLevelState switchLevelState;
    Timer switchLevelDelayTimer{30};

    psyqo::Angle interactStartAngle;
    psyqo::Angle interactEndAngle;
    psyqo::Angle interactRotationLerpFactor;
    psyqo::Angle interactRotationLerpSpeed;
    bool npcRotatesTowardsPlayer{false};

    eastl::vector<AABB> collisionBoxes;
    eastl::vector<Circle> collisionCircles;
    eastl::vector<Trigger> triggers;

    int fadeLevel = 0;
    bool fadeFinished{false};
    bool fadeOut{false}; // if false - fade in
    int destinationLevelId = 0;

    bool collisionEnabled{true};
};
