#pragma once

#include <psyqo/scene.hh>

#include <Camera.h>
#include <Core/StringHash.h>
#include <Graphics/SkeletalAnimation.h>
#include <Graphics/SkeletonAnimator.h>
#include <Math/Quaternion.h>
#include <Object.h>
#include <TileMap.h>
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
    void handleFloorCollision();
    void handleCollision(psyqo::SoftMath::Axis axis);
    void updateCamera();

    void calculateTileVisibility();

    void draw(Renderer& renderer);
    void drawTiles(Renderer& renderer);

    void drawDebugInfo(Renderer& renderer);
    void dumpDebugInfoToTTY();

    void switchLevel(int levelId);

    Game& game;

    // game objects
    AnimatedModelObject player;
    AnimatedModelObject npc;

    Camera camera;

    TextureInfo uiTexture;

    DialogueBox dialogueBox;
    DialogueBox interactionDialogueBox;

    std::size_t animIndex{1};

    FPSCounter fpsCounter;

    bool debugInfoDrawn{false};
    bool collisionDrawn{true};
    bool freeCamera{false};
    bool followCamera{false};

    bool canTalk{false};
    bool canInteract{false};

    enum class GameState { Normal, Dialogue, SwitchLevel };
    GameState gameState;

    enum class SwitchLevelState { FadeOut, LoadLevel, FadeIn, Done };
    SwitchLevelState switchLevelState;
    bool startedLevelLoad{false}; // needed to not update/draw GameplayState during level load

    psyqo::Angle interactStartAngle;
    psyqo::Angle interactEndAngle;
    psyqo::Angle interactRotationLerpFactor;
    psyqo::Angle interactRotationLerpSpeed;
    bool npcRotatesTowardsPlayer{false};

    eastl::vector<Circle> collisionCircles;
    eastl::vector<Trigger> triggers;

    int fadeLevel = 0;

    bool fadeFinished{false};
    bool fadeOut{false}; // if false - fade in
    int destinationLevelId = 0;

    bool collisionEnabled{true};

    bool cutscene{false};

    int numTilesDrawn{0};

    int minTileX, maxTileX;
    int minTileZ, maxTileZ;

    TileMap tileMap;

    psyqo::Color fogColor;
};
