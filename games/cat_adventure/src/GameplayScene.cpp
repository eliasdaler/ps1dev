#include "GameplayScene.h"

#include <common/syscalls/syscalls.h>
#include <psyqo/alloc.h>
#include <psyqo/gte-registers.hh>
#include <psyqo/primitives/lines.hh>
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/primitives/sprites.hh>
#include <psyqo/soft-math.hh>
#include <psyqo/xprintf.h>

#include <Common.h>
#include <Game.h>
#include <Graphics/Renderer.h>
#include <Math/Math.h>
#include <Math/gte-math.h>
#include <UI/TextLabel.h>

#include "Resources.h"

#include <ActionList/ActionWrappers.h>

#include <ranges>

#define DEV_TOOLS

#define NEW_CAM

uint8_t fogR = 108;
uint8_t fogG = 100;
uint8_t fogB = 116;

// This will be calling into the Lua script from execSlot 255,
// where it will be able to modify the background color of the scene.
void pcsxRegisterVariable(void* address, const char* name)
{
    register void* a0 asm("a0") = address;
    register const char* a1 asm("a1") = name;
    __asm__ volatile("" : : "r"(a0), "r"(a1));
    *((volatile uint8_t* const)0x1f802081) = 255;
}

namespace
{
constexpr auto worldScale = 8.0;
consteval long double ToWorldCoords(long double d)
{
    return d / worldScale;
}

static constexpr auto freeCameraRotPitch = psyqo::FixedPoint<10>(0.12);
static const auto freeCameraDistance = psyqo::FixedPoint<>(0.35);
static constexpr auto freeCameraOffset = psyqo::Vec3{
    .x = 0,
    .y = 0.15,
    .z = 0,
};

#ifdef NEW_CAM
static constexpr auto cameraOffset = psyqo::Vec3{
    .x = -0.1,
    .y = 0.32,
    .z = -0.40,
};
#else
static constexpr auto cameraOffset = psyqo::Vec3{
    .x = -0.10,
    .y = 0.20,
    .z = -0.42,
};
#endif

void calculateCameraRelativeStickDirection(const Camera& camera, psyqo::Vec3& stickDir)
{
    // project front onto the ground plane
    auto cameraFrontProj = camera.view.rotation.vs[2];
    cameraFrontProj.y = 0.0;
    cameraFrontProj = math::normalize(cameraFrontProj);

    static constexpr auto globalUp = psyqo::Vec3{0, 1, 0};
    /* const auto cameraRight =
        math::normalize(psyqo::SoftMath::crossProductVec3(cameraFrontProj, globalUp)); */
    const auto cameraRight =
        math::normalize({.x = -cameraFrontProj.z, .y = 0, .z = cameraFrontProj.x});

    // FIXME: this might not be the most optimized way to do stuff
    const auto rightMovement = cameraRight * stickDir.x;
    const auto upMovement = cameraFrontProj * (-stickDir.y);

    stickDir.x = upMovement.x + rightMovement.x;
    stickDir.y = upMovement.z + rightMovement.z;

    const auto temp = math::normalize({stickDir.x, 0.0, stickDir.y});
    stickDir.x = temp.x;
    stickDir.y = temp.z;
}

} // end of anonymous namespace

GameplayScene::GameplayScene(Game& game) : game(game)
{}

void GameplayScene::start(StartReason reason)
{
    if (reason == StartReason::Create) {
        pcsxRegisterVariable(&game.renderer.fogColor, "game.renderer.fogColor");

        game.renderer.setFogNearFar(0.2, 1.2);
        static const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
        game.renderer.setFarColor(farColor);

        static constexpr auto shFogColor = psyqo::Color{.r = 108, .g = 100, .b = 116};
        game.renderer.setFogColor(shFogColor);

        // game.renderer.setFogNearFar(0.1, 1.8);
        // game.renderer.setFogColor({.r = 0, .g = 32, .b = 32});

        player.model = game.resourceCache.getResource<ModelData>(CATO_MODEL_HASH).makeInstance();

        // FIXME: somehow mark face mesh in Blender
        player.faceSubmeshIdx = 0xFF;
        for (int i = 0; i < player.model.meshes.size(); ++i) {
            const auto& submesh = *player.model.meshes[i].meshData;
            if (submesh.gt4.size() == 16) {
                player.faceSubmeshIdx = i;
            }
        }

        player.setFaceAnimation(DEFAULT_FACE_ANIMATION);
        player.blinkPeriod = 140;
        player.closedEyesTime = 4;
        player.blinkTimer.reset(player.blinkPeriod);

        player.jointGlobalTransforms.resize(player.model.armature.joints.size());
        player.animator.animations = &game.catAnimations;

        game.songPlayer.init(game.midi, game.vab);

        initUI();
        initDebugMenu();
    }

    /* triggers.clear();

    if (game.level.id == 0) { // TODO: load from level
        Trigger trigger;
        trigger.id = 0;
        trigger.aabb.min = {-0.1145, 0.0000, 0.3488};
        trigger.aabb.max = {0.0698, 0.1000, 0.4375};
        triggers.push_back(trigger);

        trigger.id = 1;
        trigger.interaction = true;
        trigger.aabb.min = {-0.2775, 0.0000, 0.07};
        trigger.aabb.max = {-0.1821, 0.0000, 0.22};
        triggers.push_back(trigger);
    } else {
        Trigger trigger;
        trigger.id = 0;
        trigger.aabb.min = {-0.1145, 0.0000, 0.3488};
        trigger.aabb.max = {0.0698, 0.1000, 0.4375};
        triggers.push_back(trigger);
    } */

    npc.model = game.resourceCache.getResource<ModelData>(HUMAN_MODEL_HASH).makeInstance();
    npc.jointGlobalTransforms.resize(npc.model.armature.joints.size());
    npc.animator.animations = &game.humanAnimations;
    npc.animator.setAnimation("Idle"_sh);

    if (game.level.id == 0) {
        game.renderer.setFogEnabled(false);

        player.setPosition({-0.0439, -0.0200, 0.2141});
        player.setYaw(1.0);

        npc.setPosition({0.0, 0.0, -0.11});
        npc.setYaw(0.1);

        camera.position = {0.2900, 0.4501, 0.5192};
        camera.rotation = {0.1621, -0.7753};

        followCamera = false;
    } else if (game.level.id == 1) {
        game.renderer.setFogEnabled(true);
        // game.renderer.setFogEnabled(false);

        player.setPosition({-0.1083, 0.0000, -1.2602});
        npc.setYaw(-0.01);

        // cutscene test
        player.setPosition({-0.4472, 0.0000, -1.2111});
        player.setYaw(0.54);

        npc.setPosition({-0.3344, 0.0000, -1.2253});
        npc.setYaw(-1.71);

        followCamera = true;
    }

    canTalk = false;
    game.activeInteractionTriggerIdx = -1;

    player.animator.setAnimation("Idle"_sh);
    player.model.armature.selectedJoint = 4;
}

void GameplayScene::initUI()
{
    uiTexture = game.resourceCache.getResource<TextureInfo>(CATO_TEXTURE_HASH);

    interactionDialogueBox.displayBorders = false;
    interactionDialogueBox.textOffset.x = 48;
    interactionDialogueBox.position.y = 180;
    interactionDialogueBox.size.y = 32;
    interactionDialogueBox.displayMoreTextArrow = false;
}

void GameplayScene::initDebugMenu()
{
    game.debugMenu.menuItems[DebugMenu::COLLISION_ITEM_ID].valuePtr = &collisionEnabled;
    game.debugMenu.menuItems[DebugMenu::FOLLOW_CAMERA_ITEM_ID].valuePtr = &followCamera;
    game.debugMenu.menuItems[DebugMenu::MUTE_MUSIC_ITEM_ID].valuePtr = &game.songPlayer.musicMuted;
    game.debugMenu.menuItems[DebugMenu::DRAW_COLLISION_ITEM_ID].valuePtr = &collisionDrawn;
}

void GameplayScene::frame()
{
    game.handleDeltas();

    game.actionListManager.update(game.frameDtMcs, false);

    fpsCounter.update(game.gpu());
    game.pad.update();
    processInput(game.pad);
    update();

    if (startedLevelLoad) {
        return;
    }

    gpu().pumpCallbacks();

    draw(game.renderer);

    game.onFrameEnd();
}

void GameplayScene::processInput(const PadManager& pad)
{
#ifdef DEV_TOOLS
    if ((pad.isButtonHeld(psyqo::SimplePad::Square) &&
            pad.wasButtonJustPressed(psyqo::SimplePad::Triangle)) ||
        (pad.wasButtonJustPressed(psyqo::SimplePad::Square) &&
            pad.isButtonHeld(psyqo::SimplePad::Triangle))) {
        game.debugMenu.open = !game.debugMenu.open;
    }

    if (game.debugMenu.open) {
        auto selectedItemIdx = game.debugMenu.processInput(pad);
        switch (selectedItemIdx) {
        case DebugMenu::DUMP_DEBUG_INFO_ITEM_ID:
            dumpDebugInfoToTTY();
            break;
        case DebugMenu::FOLLOW_CAMERA_ITEM_ID:
            if (!followCamera) {
                camera.position = {0.2900, 0.4501, 0.5192};
                camera.rotation = {0.1621, -0.7753};
            }
            break;
        case DebugMenu::MUTE_MUSIC_ITEM_ID:
            if (game.songPlayer.musicMuted) {
                game.songPlayer.pauseMusic();
            }
            break;
        }

        return;
    }
#endif

    if (freeCamera) {
        processFreeCameraInput(pad);
    } else if (gameState == GameState::Normal) {
        processPlayerInput(pad);
    } else if (gameState == GameState::Dialogue) {
        if (dialogueBox.isOpen) {
            dialogueBox.handleInput(pad);
            if (dialogueBox.wantClose) {
                // gameState = GameState::Normal;
            }
        }
    }

    processDebugInput(pad);
}

void GameplayScene::processPlayerInput(const PadManager& pad)
{
    const auto& trig = game.trig;

    constexpr auto walkSpeed = psyqo::FixedPoint<>(0.15);
    constexpr auto sprintSpeed = psyqo::FixedPoint<>(0.4);
    constexpr auto rotateSpeed = psyqo::FixedPoint<>(1.0);
    const auto dt = game.frameDt;

    // yaw
    bool isRotating = false;
    // go forward/backward
    bool isMoving = false;
    bool moveForward = false;
    bool isSprinting = false;

    if (pad.isButtonHeld(psyqo::SimplePad::Square)) {
        isSprinting = true;
    }

    // #define TANK_CONTROLS

#ifdef TANK_CONTROLS
    freeCameraRotation = false;
    if (pad.isButtonPressed(psyqo::SimplePad::Left)) {
        const auto newAngle = player.getYaw() + psyqo::Angle(rotateSpeed * dt);
        player.setYaw(newAngle);
        isRotating = true;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Right)) {
        const auto newAngle = player.getYaw() - psyqo::Angle(rotateSpeed * dt);
        player.setYaw(newAngle);
        isRotating = true;
    }

    if (pad.isButtonPressed(psyqo::SimplePad::Up)) {
        isMoving = true;
        moveForward = true;
    }

    if (pad.isButtonPressed(psyqo::SimplePad::Down)) {
        isMoving = true;
        moveForward = false;
    }

    player.velocity = {};

    if (isMoving) {
        psyqo::FixedPoint<> speed{};
        if (moveForward) {
            if (isSprinting) {
                speed = sprintSpeed;
            } else {
                speed = walkSpeed;
            }
        } else {
            speed = -walkSpeed * 0.4;
        }
        player.velocity = player.getFront() * speed;
    }
#else
    psyqo::Vec3 stickDir{};

    if (pad.isButtonHeld(psyqo::SimplePad::Left)) {
        stickDir.x -= 1.0;
    }

    if (pad.isButtonHeld(psyqo::SimplePad::Right)) {
        stickDir.x += 1.0;
    }

    if (pad.isButtonHeld(psyqo::SimplePad::Up)) {
        stickDir.y -= 1.0;
    }

    if (pad.isButtonHeld(psyqo::SimplePad::Down)) {
        stickDir.y += 1.0;
    }

    isMoving = (stickDir.x != 0.0 || stickDir.y != 0.0);
    if (isMoving) {
        // "normalize" dpad input if moving by a diagonal
        if (stickDir.x != 0.0 && stickDir.y != 0.0) {
            static const auto oneDivBySqrt2 = psyqo::FixedPoint<>(0.70710678118);
            stickDir.x = stickDir.x > 0 ? oneDivBySqrt2 : -oneDivBySqrt2;
            stickDir.y = stickDir.y > 0 ? oneDivBySqrt2 : -oneDivBySqrt2;
        }

        calculateCameraRelativeStickDirection(camera, stickDir);
        player.rotateTowards(math::atan2(stickDir.x, stickDir.y),
            isSprinting ? psyqo::FixedPoint<>(3.0) : psyqo::FixedPoint<>(2.0));
    } else {
        player.stopRotation();
    }

    isRotating = player.isRotating();

    player.velocity = {};

    if (isMoving) {
        moveForward = true;
        const auto& speed = isSprinting ? sprintSpeed : walkSpeed;
        player.velocity.x = stickDir.x * speed;
        player.velocity.z = stickDir.y * speed;
    }
#endif

    if (isMoving) {
        if (moveForward) {
            if (isSprinting) {
                player.animator.setAnimation("Run"_sh, 1.0, 0.125);
            } else {
                player.animator.setAnimation("Walk"_sh, 1.0, 0.3);
            }
        } else {
            player.animator.setAnimation("Walk"_sh, -0.6, 0.3);
        }
    } else if (isRotating) {
        player.animator.setAnimation("Walk"_sh, 0.7, 0.3);
    } else {
        if (!cutscene) {
            player.animator.setAnimation("Idle"_sh);
        }
    }

    if (cutscene && player.animator.getCurrentAnimationName() == "ThinkStart"_sh &&
        player.animator.animationJustEnded()) {
        player.animator.setAnimation("Think"_sh);
    }

    // handle walking sounds
    if (isMoving || isRotating) {
        if (player.animator.frameJustChanged()) {
            const auto animFrame = player.animator.getAnimationFrame();
            // TODO: write better code
            const auto playerTileIndex = TileMap::getTileIndex(player.getPosition());
            const auto tileInfo = game.level.tileMap.getTile(playerTileIndex);
            bool onGrass = (tileInfo.tileId == 3);

            if (player.animator.getCurrentAnimationName() == "Walk"_sh) {
                if (animFrame == 3) {
                    game.soundPlayer.playSound(20, onGrass ? game.gstep1Sound : game.step1Sound);
                } else if (animFrame == 15) {
                    game.soundPlayer.playSound(21, onGrass ? game.gstep2Sound : game.step2Sound);
                }
            } else if (player.animator.getCurrentAnimationName() == "Run"_sh) {
                if (animFrame == 2) {
                    game.soundPlayer.playSound(20, onGrass ? game.gstep1Sound : game.step1Sound);
                } else if (animFrame == 10) {
                    game.soundPlayer.playSound(21, onGrass ? game.gstep2Sound : game.step2Sound);
                }
            }
        }
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Cross)) {
        if (canTalk) {
            npcInteractSay(npc, "Hello...\nNice weather today,\nisn't it?");
        } else if (game.activeInteractionTriggerIdx != -1) {
            const auto& trigger = game.level.triggers[game.activeInteractionTriggerIdx];
            if (trigger.name == "TV"_sh) {
                gameState = GameState::Dialogue;
                player.animator.setAnimation("Idle"_sh);
                dialogueBox.setText("You see a \3\6gleeby-\ndeeby\1\3 on the\nscreen...");
            }
        }
    }
}

void GameplayScene::processFreeCameraInput(const PadManager& pad)
{
    const auto& trig = game.trig;

    constexpr auto walkSpeed = psyqo::FixedPoint<>(2.0);
    constexpr auto rotateSpeed = psyqo::FixedPoint<>(0.8);
    const auto dt = game.frameDt;

    // yaw
    if (pad.isButtonPressed(psyqo::SimplePad::Left)) {
        camera.rotation.y += psyqo::Angle(rotateSpeed * dt);
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Right)) {
        camera.rotation.y -= psyqo::Angle(rotateSpeed * dt);
    }

    // pitch
    if (pad.isButtonPressed(psyqo::SimplePad::L2)) {
        camera.rotation.x += psyqo::Angle(rotateSpeed * dt * 0.6);
    }
    if (pad.isButtonPressed(psyqo::SimplePad::R2)) {
        camera.rotation.x -= psyqo::Angle(rotateSpeed * dt * 0.6);
    }

    // go up/down
    if (pad.isButtonPressed(psyqo::SimplePad::L1)) {
        camera.position.y -= walkSpeed * dt * 0.5;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::R1)) {
        camera.position.y += walkSpeed * dt * 0.5;
    }

    // go forward/backward
    if (pad.isButtonPressed(psyqo::SimplePad::Up)) {
        camera.position.x += trig.sin(camera.rotation.y) * walkSpeed * dt;
        camera.position.z += trig.cos(camera.rotation.y) * walkSpeed * dt;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Down)) {
        camera.position.x -= trig.sin(camera.rotation.y) * walkSpeed * dt;
        camera.position.z -= trig.cos(camera.rotation.y) * walkSpeed * dt;
    }
}

void GameplayScene::processDebugInput(const PadManager& pad)
{
    if (pad.wasButtonJustPressed(psyqo::SimplePad::Select)) {
        freeCamera = !freeCamera;
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Start)) {
        debugInfoDrawn = !debugInfoDrawn;
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Circle)) {
        if (!game.actionListManager.isActionListPlaying("test"_sh)) {
            playTestCutscene();
        }

        /* if (!cutscene) {
            player.setFaceAnimation(ANGRY_FACE_ANIMATION);
            player.animator.setAnimation("ThinkStart"_sh);
            cutscene = true;
        } else {
            player.setFaceAnimation(DEFAULT_FACE_ANIMATION);
            player.animator.setAnimation("Idle"_sh);
            cutscene = false;
        } */
    }
}

void GameplayScene::updateCamera()
{
    const auto& trig = game.trig;

    if (gameState != GameState::Dialogue) {
        if (!freeCamera && followCamera && !freeCameraRotation) {
#ifdef NEW_CAM
            static constexpr auto cameraPitch = psyqo::FixedPoint<10>(0.12);
#else
            static constexpr auto cameraPitch = psyqo::FixedPoint<10>(0.035);
#endif
            camera.rotation.x = cameraPitch;
            camera.rotation.y = player.getYaw();
        }

        if (!freeCamera && followCamera && freeCameraRotation) {
            constexpr auto rotateSpeed = psyqo::FixedPoint<>(0.8);
            auto& pad = game.pad;
            const auto dt = game.frameDt;
            if (pad.isButtonPressed(psyqo::SimplePad::L1)) {
                camera.rotation.y += psyqo::Angle(rotateSpeed * dt);
            }
            if (pad.isButtonPressed(psyqo::SimplePad::R1)) {
                camera.rotation.y -= psyqo::Angle(rotateSpeed * dt);
            }

            camera.rotation.x = freeCameraRotPitch;
        }
    }

    calculateViewMatrix(&camera.view.rotation, camera.rotation.x, camera.rotation.y, trig);

    if (gameState != GameState::Dialogue && followCamera && !freeCamera) {
        camera.position = getCameraDesiredPosition(camera.rotation.x, camera.rotation.y);
    }

    camera.view.translation = -camera.position;
    // We do this on CPU because -camera.position can be outside of 4.12 fixed point range
    psyqo::SoftMath::matrixVecMul3(
        camera.view.rotation, camera.view.translation, &camera.view.translation);
}

psyqo::Vec3 GameplayScene::getCameraDesiredPosition(psyqo::Angle pitch,
    psyqo::Angle yaw,
    bool useCurrentCameraRotMatrix)
{
    psyqo::Vec3 desiredPosition{};

    if (followCamera && !freeCameraRotation) {
        const auto fwdVector = player.getFront();
        const auto rightVector = player.getRight();

        const auto& playerPos = player.getPosition();
        desiredPosition.y = playerPos.y + cameraOffset.y;
        desiredPosition.x =
            playerPos.x + fwdVector.x * cameraOffset.z + rightVector.x * cameraOffset.x;
        desiredPosition.z =
            playerPos.z + fwdVector.z * cameraOffset.z + rightVector.z * cameraOffset.x;

        // for now set the camera at constant height so that the curbs feel better
#ifdef NEW_CAM
        desiredPosition.y = 0.32;
#else
        desiredPosition.y = 0.20;
#endif
    } else {
        psyqo::Vec3 cameraFront;
        if (useCurrentCameraRotMatrix) {
            cameraFront = camera.view.rotation.vs[2];
        } else {
            // see calculateViewMatrix
            const auto sx = game.trig.sin(pitch);
            const auto cx = game.trig.cos(pitch);
            const auto sy = game.trig.sin(yaw);
            const auto cy = game.trig.cos(yaw);
            cameraFront.x = cx * sy;
            cameraFront.y = -sx;
            cameraFront.z = cx * cy;
        }

        desiredPosition =
            player.getPosition() + freeCameraOffset - cameraFront * freeCameraDistance;
    }

    return desiredPosition;
}

void GameplayScene::update()
{
    if (gameState == GameState::Normal) {
        npc.updateCollision();

        if (collisionEnabled) {
            handleCollision(psyqo::SoftMath::Axis::X);
            handleCollision(psyqo::SoftMath::Axis::Z);
        }

        auto pos = player.getPosition();
        pos += player.velocity * game.frameDt;
        player.setPosition(pos);

        if (game.level.id == 1) {
            handleFloorCollision();
        }

        player.updateCollision();
        updateTriggers();
    }

    if (gameState == GameState::SwitchLevel) {
        updateLevelSwitch();
        if (startedLevelLoad) {
            return;
        }
    }

    player.update(game.frameDtMcs);
    npc.update(game.frameDtMcs);

    updateCamera();

    if (gameState == GameState::Normal) {
        canTalk = circlesIntersect(player.interactionCircle, npc.collisionCircle);
    } else if (gameState == GameState::Dialogue) {
        if (dialogueBox.isOpen) {
            dialogueBox.update();
        }
    }
}

void GameplayScene::updateTriggers()
{
    if (!collisionEnabled) {
        return;
    }

    game.activeInteractionTriggerIdx = -1;

    auto& triggers = game.level.triggers;
    for (auto [idx, trigger] : std::views::enumerate(triggers)) {
        trigger.wasEntered = trigger.isEntered;

        if (trigger.interaction) {
            trigger.isEntered = circleAABBIntersect(player.interactionCircle, trigger.aabb);
            if (trigger.isEntered) {
                game.activeInteractionTriggerIdx = idx;
            }
        } else {
            trigger.isEntered = pointInAABB(trigger.aabb, player.getPosition());
        }

        if (trigger.wasJustEntered()) {
            if (trigger.name == "HouseExit"_sh) {
                switchLevel(1);
            } else if (trigger.name == "HouseEnter"_sh) {
                switchLevel(0);
            }
        }
    }
}

void GameplayScene::updateLevelSwitch()
{
    if (!fadeFinished) {
        if (fadeOut) {
            fadeLevel += 10;
            if (fadeLevel > 255) {
                fadeLevel = 255;
                fadeFinished = true;
            }
        } else { // fade in
            fadeLevel -= 10;
            if (fadeLevel < 0) {
                fadeLevel = 0;
                fadeFinished = true;
            }
        }
    }

    switch (switchLevelState) {
    case SwitchLevelState::FadeOut:
        if (fadeFinished) {
            switchLevelState = SwitchLevelState::LoadLevel;
        }
        break;
    case SwitchLevelState::LoadLevel:
        // switch level
        startedLevelLoad = true;
        game.loadLevel(destinationLevelId);

        fadeFinished = false;
        fadeOut = false;
        fadeLevel = 255;
        switchLevelState = SwitchLevelState::FadeIn;
        break;
    case SwitchLevelState::FadeIn:
        startedLevelLoad = false;
        if (fadeFinished) {
            switchLevelState = SwitchLevelState::Done;
        }
        break;
    case SwitchLevelState::Done:
        gameState = GameState::Normal;
        break;
    }
}

void GameplayScene::handleFloorCollision()
{
    const auto playerTileIndex = TileMap::getTileIndex(player.getPosition());
    const auto tileInfo = game.level.tileMap.getTile(playerTileIndex);

    const auto tileId = tileInfo.tileId;
    if (tileId == 0 || tileId == 1 || tileId == 2) {
        // on road tile
        player.transform.translation.y = -0.02;
    } else {
        player.transform.translation.y = 0.0;
    }

    // on curb transition tile
    if (tileId == 5 || tileId == 6) {
        psyqo::FixedPoint lerpF = 0.0;
        // 0.02 / 0.125 = 0.16 (16% of curb width is where we reach the max height)
        static constexpr auto curbMaxHeightPoint = 0.02;

        // essentially we're doing lerpF = (x - a) / (b - a),
        // so curbMaxHeightPointScale is our 1/ ( b - a)
        static constexpr auto curbMaxHeightPointScale = 50; // 1 / 0.02

        if (playerTileIndex.z > 0) {
            lerpF = (psyqo::FixedPoint<>(playerTileIndex.z, 0) * Tile::SCALE + curbMaxHeightPoint -
                        player.transform.translation.z) *
                    curbMaxHeightPointScale;
        } else {
            lerpF = (player.transform.translation.z -
                        (psyqo::FixedPoint(playerTileIndex.z + 1, 0) * Tile::SCALE -
                            curbMaxHeightPoint)) *
                    curbMaxHeightPointScale;
        }

        static constexpr auto zero = psyqo::FixedPoint<>(0.0);
        static constexpr auto one = psyqo::FixedPoint<>(1.0);
        lerpF = eastl::clamp(lerpF, zero, one);

        static constexpr auto roadHeight = psyqo::FixedPoint<>(-0.02); // TODO: get from tile info
        player.transform.translation.y = math::lerp(0.0, roadHeight, lerpF);
    }
}

void GameplayScene::handleCollision(psyqo::SoftMath::Axis axis)
{
    player.updateCollision();

    // Move on one of the axes first
    const auto oldPos = player.getPosition();
    bool anyCollision = false;
    auto newPos = oldPos;
    if (axis == psyqo::SoftMath::Axis::X) {
        newPos.x += player.velocity.x * game.frameDt;
    } else if (axis == psyqo::SoftMath::Axis::Z) {
        newPos.z += player.velocity.z * game.frameDt;
    }
    player.setPosition(newPos);
    player.updateCollision();

    const auto& collisionBoxes = game.level.collisionBoxes;

    // Check if any collisions happened
    if (circlesIntersect(player.collisionCircle, npc.collisionCircle)) {
        anyCollision = true;
        goto collidedWithSomething;
    }

    for (const auto& testBox : collisionBoxes) {
        if (circleAABBIntersect(player.collisionCircle, testBox)) {
            anyCollision = true;
            goto collidedWithSomething;
        }
    }

collidedWithSomething:
    if (anyCollision) {
        // If they did - set velocity to 0 on that axis so that the player
        // doesn't move in the direction of the collision
        if (axis == psyqo::SoftMath::Axis::X) {
            player.velocity.x = {};
        } else if (axis == psyqo::SoftMath::Axis::Z) {
            player.velocity.z = {};
        }
    }

    // Restore the old position (velocity will be applied later)
    player.setPosition(oldPos);
}

void GameplayScene::draw(Renderer& renderer)
{
    auto& ot = renderer.getOrderingTable();
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gp = gpu();

    primBuffer.reset();

    // set dithering ON globally
    auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
    tpage.primitive.attr.setDithering(true);
    gp.chain(tpage);

    {
        auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
            psyqo::Prim::MaskControl::Set::FromSource, psyqo::Prim::MaskControl::Test::No);
        gp.chain(maskBit);
    }

    // clear (TODO: make a level property?)
    // psyqo::Color bg{{.r = 0, .g = 0, .b = 0}};
    psyqo::Color bg{{.r = 33, .g = 14, .b = 58}};

    if (!renderer.isFogEnabled()) {
        auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
        gp.getNextClear(fill.primitive, bg);
        gp.chain(fill);
    }

#if 0 // sunset sky bg
    {
#ifdef NEW_CAM
        auto gradY1 = 60;
#else
        auto gradY1 = 100;
#endif

        psyqo::Color bg1{{.r = 40, .g = 38, .b = 100}};
        psyqo::Color bg2{{.r = 255, .g = 128, .b = 0}};
        {
            auto& prim = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
            auto& q = prim.primitive;
            q.setColorA(bg1);
            q.setColorB(bg1);
            q.setColorC(bg2);
            q.setColorD(bg2);
            q.pointA.x = 0;
            q.pointA.y = 0;
            q.pointB.x = SCREEN_WIDTH;
            q.pointB.y = 0;
            q.pointC.x = 0;
            q.pointC.y = gradY1;
            q.pointD.x = SCREEN_WIDTH;
            q.pointD.y = gradY1;
            gpu().chain(prim);
        }

        /* {
            psyqo::Color bg3{{.r = 0, .g = 0, .b = 0}};
            auto& prim = primBuffer.allocateFragment<psyqo::Prim::Quad>();
            auto& q = prim.primitive;
            q.setColor(bg3);
            q.pointA.x = 0;
            q.pointA.y = gradY1;
            q.pointB.x = SCREEN_WIDTH;
            q.pointB.y = gradY1;
            q.pointC.x = 0;
            q.pointC.y = SCREEN_HEIGHT;
            q.pointD.x = SCREEN_WIDTH;
            q.pointD.y = SCREEN_HEIGHT;
            gpu().chain(prim);
        } */
    }
#endif

    if (game.renderer.isFogEnabled()) {
        auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
            psyqo::Prim::MaskControl::Set::ForceSet, psyqo::Prim::MaskControl::Test::No);
        gp.chain(maskBit);

        const auto fogColor = game.renderer.getFogColor();
        auto& quadFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
        auto& q = quadFrag.primitive;
        q.pointA.x = 0;
        q.pointA.y = 0;
        q.pointB.x = SCREEN_WIDTH;
        q.pointB.y = 0;
        q.pointC.x = 0;
        q.pointC.y = SCREEN_HEIGHT;
        q.pointD.x = SCREEN_WIDTH;
        q.pointD.y = SCREEN_HEIGHT;
        q.setColorA(fogColor);
        q.colorB = fogColor;
        q.colorC = fogColor;
        q.colorD = fogColor;

        gp.chain(quadFrag);
    }

    {
        auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
            psyqo::Prim::MaskControl::Set::FromSource, psyqo::Prim::MaskControl::Test::No);
        gp.chain(maskBit);
    }

    {
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr.setDithering(true).set(
            psyqo::Prim::TPageAttr::SemiTrans::FullBackAndFullFront);
        gp.chain(tpage);
    }

    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);

    if (game.level.id == 1) {
        renderer.drawTiles(game.level.modelData, game.level.tileMap, camera);
    }

    gp.pumpCallbacks();

    // draw static objects without rotation (R won't be changed)
    for (auto& staticObject : game.level.staticObjects) {
        if (!staticObject.hasRotation()) {
            renderer.drawMeshObject(staticObject, camera, false);
        }
    }

    // draw static objects without rotation (R will be changed)
    for (auto& staticObject : game.level.staticObjects) {
        if (staticObject.hasRotation()) {
            renderer.drawMeshObject(staticObject, camera, true);
        }
    }

    gp.pumpCallbacks();

    // draw dynamic objects
    {
        renderer.drawAnimatedModelObject(player, camera);
        renderer.drawAnimatedModelObject(npc, camera);
    }

    gp.pumpCallbacks();

    gp.chain(ot);

    if (fadeLevel != 0) { // draw fade in/out
        auto& gpu = renderer.getGPU();

        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr.set(psyqo::Prim::TPageAttr::SemiTrans::FullBackSubFullFront);
        gpu.chain(tpage);

        for (int i = 0; i < 4; ++i) {
            // draw 4 rects - for some reason PS1 can't handle blending the whole
            // screen properly on real HW
            auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
            auto& rect = rectFrag.primitive;
            rect.position = {};
            switch (i) {
            case 1:
                rect.position = {.x = SCREEN_WIDTH / 2, .y = 0};
                break;
            case 2:
                rect.position = {.x = 0, .y = SCREEN_HEIGHT / 2};
                break;
            case 3:
                rect.position = {.x = SCREEN_WIDTH / 2, .y = SCREEN_HEIGHT / 2};
                break;
            }
            rect.size.x = SCREEN_WIDTH / 2;
            rect.size.y = SCREEN_HEIGHT / 2;

            const auto black =
                psyqo::Color{{uint8_t(fadeLevel), uint8_t(fadeLevel), uint8_t(fadeLevel)}};
            rect.setColor(black);
            rect.setSemiTrans();
            gpu.chain(rectFrag);
        }
    }

    {
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr.setDithering(false);
        gp.chain(tpage);
    }

    if (gameState == GameState::Normal) {
        if (canTalk) {
            interactionDialogueBox.setText("\5(X)\1 Talk", true);
            interactionDialogueBox.draw(renderer, game.font, game.fontTexture, uiTexture);
        } else if (game.activeInteractionTriggerIdx != -1) {
            interactionDialogueBox.setText("\5(X)\1 Interact", true);
            interactionDialogueBox.draw(renderer, game.font, game.fontTexture, uiTexture);
        }
    }

    if (cutsceneBorderHeight != -1) { // draw dialogue screen borders
        auto& gpu = renderer.getGPU();

        if (cutsceneStart) {
            cutsceneBorderHeight += 2;
        } else {
            cutsceneBorderHeight -= 2;
        }

        static constexpr int maxCutsceneBorderHeight = 24;
        cutsceneBorderHeight = eastl::clamp(cutsceneBorderHeight, -1, maxCutsceneBorderHeight);
        if (cutsceneBorderHeight != -1) {
            { // top
                auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
                auto& rect = rectFrag.primitive;
                rect.position = {};
                rect.size.x = SCREEN_WIDTH;
                rect.size.y = cutsceneBorderHeight;

                rect.setColor({}); // black
                gpu.chain(rectFrag);
            }

            { // bottom
                auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
                auto& rect = rectFrag.primitive;
                rect.position = {.y = (std::int16_t)(SCREEN_HEIGHT - cutsceneBorderHeight)};
                rect.size.x = SCREEN_WIDTH;
                rect.size.y = cutsceneBorderHeight;

                rect.setColor({}); // black
                gpu.chain(rectFrag);
            }
        }
    }

    if (gameState == GameState::Dialogue && dialogueBox.isOpen) {
        dialogueBox.draw(renderer, game.font, game.fontTexture, uiTexture);
    }

    /*
    renderer.drawLineWorldSpace(camera,
        player.getPosition(),
        player.getPosition() + cameraFront * 0.5,
        {.r = 255, .g = 0, .b = 0});

    renderer.drawLineWorldSpace(camera,
        player.getPosition(),
        player.getPosition() + cameraRight * 0.5,
        {.r = 0, .g = 255, .b = 0});

    renderer.drawLineWorldSpace(camera,
        player.getPosition(),
        player.getPosition() + psyqo::Vec3{stickDir.x, 0.0, stickDir.y} * 0.5,
        {.r = 0, .g = 0, .b = 255});
        */

    if (debugInfoDrawn) {
        drawDebugInfo(renderer);
    }

    /* static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
    game.romFont.chainprintf(game.gpu(),
        {{.x = 16, .y = 64}},
        textCol,
        "X: %d, Y: %d, pad type: %d",
        game.pad.getLeftAxisX(),
        game.pad.getLeftAxisY(),
        game.pad.getPadType()); */

    game.debugMenu.draw(renderer);
}

void GameplayScene::drawDebugInfo(Renderer& renderer)
{
    if (collisionDrawn) { // draw collisions/triggers
        renderer.drawObjectAxes(player, camera);

        renderer.drawArmature(player, camera);

        if (game.level.id == 0) {
            renderer.drawArmature(npc, camera);
        }

        static const auto colliderColor = psyqo::Color{.r = 128, .g = 255, .b = 255};
        static const auto triggerColor = psyqo::Color{.r = 255, .g = 255, .b = 128};
        static const auto interactionTriggerColor = psyqo::Color{.r = 255, .g = 128, .b = 128};
        static const auto activeTriggerColor = psyqo::Color{.r = 128, .g = 255, .b = 128};

        const auto& collisionBoxes = game.level.collisionBoxes;
        for (const auto& box : collisionBoxes) {
            renderer.drawAABB(camera, box, colliderColor);
        }

        for (const auto& trigger : game.level.triggers) {
            const auto& col = trigger.isEntered ?
                                  activeTriggerColor :
                                  ((trigger.interaction) ? interactionTriggerColor : triggerColor);
            renderer.drawAABB(camera, trigger.aabb, col);
        }

        renderer.drawCircle(
            camera, player.collisionCircle, psyqo::Color{.r = 255, .g = 0, .b = 255});
        renderer.drawCircle(camera, npc.collisionCircle, psyqo::Color{.r = 0, .g = 255, .b = 255});

        if (canTalk || game.activeInteractionTriggerIdx != -1) {
            renderer.drawCircle(
                camera, player.interactionCircle, psyqo::Color{.r = 255, .g = 255, .b = 0});
        } else {
            renderer.drawCircle(
                camera, player.interactionCircle, psyqo::Color{.r = 255, .g = 0, .b = 0});
        }
    }

    if (!game.debugMenu.open) {
        static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};

        const auto stateStr = [this]() {
            switch (gameState) {
            case GameState::Normal:
                return "n";
            case GameState::Dialogue:
                return "d";
            case GameState::SwitchLevel:
                return "sl";
            }
            return "";
        }();

        const auto playerTileIndex = TileMap::getTileIndex(player.getPosition());

        /* game.romFont.chainprintf(game.gpu(),
            {{.x = 16, .y = 32}},
            textCol,
            "(%.2f, %.2f, %.2f), (%.2f, %.2f)",
            camera.position.x,
            camera.position.y,
            camera.position.z,
            camera.rotation.x,
            camera.rotation.y); */

        auto& rot = player.model.armature.joints[4].localTransform.rotation;
        game.romFont.chainprintf(game.gpu(),
            {{.x = 16, .y = 32}},
            textCol,
            "(%.2f, %.2f, %.2f, %.2f)",
            psyqo::FixedPoint<>(rot.x),
            psyqo::FixedPoint<>(rot.y),
            psyqo::FixedPoint<>(rot.z),
            psyqo::FixedPoint<>(rot.w));

        int currActionIdx = 0;
        if (game.actionListManager.isActionListPlaying("interact"_sh)) {
            const auto& l = game.actionListManager.getActionList("interact"_sh);
            currActionIdx = l.getCurrentActionIndex();
        }
        game.romFont.chainprintf(
            game.gpu(), {{.x = 16, .y = 16}}, textCol, "gs =  %s, i = %d", stateStr, currActionIdx);

        /*
        game.romFont.chainprintf(game.gpu(),
            {{.x = 16, .y = 16}},
            textCol,
            "pos = (%.2f, %.4f, %.2f)",
            player.getPosition().x,
            player.getPosition().y,
            player.getPosition().z);
*/

        /* game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 32}},
            textCol,
            "(%d, %d)",
            playerTileIndex.x,
            playerTileIndex.z); */

        /* game.romFont.chainprintf(
            game.gpu(), {{.x = 16, .y = 32}}, textCol, "yaw=(%.2a)", player.getYaw()); */

        game.romFont.chainprintf(game.gpu(),
            {{.x = 16, .y = 64}},
            textCol,
            "heap used: %d",
            (int)((uint8_t*)psyqo_heap_end() - (uint8_t*)psyqo_heap_start()));

        game.romFont.chainprintf(game.gpu(),
            {{.x = 16, .y = 80}},
            textCol,
            "pb used: %d, tiles drawn: %d",
            (int)renderer.getPrimBuffer().used(),
            renderer.numTilesDrawn);

        game.romFont.chainprintf(game.gpu(),
            {{.x = 16, .y = 48}},
            textCol,
            "FPS: %.2f, fd: %d",
            fpsCounter.getMovingAverage(),
            fadeLevel);
    }
}

void GameplayScene::dumpDebugInfoToTTY()
{
    static eastl::fixed_string<char, 512> str;

    // dump camera position/rotation
    fsprintf(str,
        "camera.position = {%.4f, %.4f, %.4f};\ncamera.rotation = {%.4f, %.4f};",
        camera.position.x,
        camera.position.y,
        camera.position.z,
        psyqo::FixedPoint<>(camera.rotation.x),
        psyqo::FixedPoint<>(camera.rotation.y));
    ramsyscall_printf("%s\n", str.c_str());

    // dump player position/rotation
    fsprintf(str,
        "player.setPosition({%.4f, %.4f, %.4f});\nplayer.setYaw(%.2a);",
        player.transform.translation.x,
        player.transform.translation.y,
        player.transform.translation.z,
        player.getYaw());
    ramsyscall_printf("%s\n", str.c_str());
}

void GameplayScene::switchLevel(int levelId)
{
    gameState = GameState::SwitchLevel;
    switchLevelState = SwitchLevelState::FadeOut;
    fadeLevel = 0;
    fadeOut = true;
    fadeFinished = false;
    destinationLevelId = levelId;

    if (game.level.id == 1) {
        destinationLevelId = 0;
    }
}

void GameplayScene::playTestCutscene()
{
    static constexpr auto camNPC = CameraTransform{
        .position = {0.1376, 0.1318, 0.0236},
        .rotation = {-0.0410, -0.7060},
    };

    static constexpr auto camNPC2 = CameraTransform{
        .position = {0.0292, 0.1381, 0.0100},
        .rotation = {-0.1650, -0.9492},
    };

    static constexpr auto camPlayer = CameraTransform{
        .position = {0.0754, 0.0390, -0.0578},
        .rotation = {-0.0390, -0.2050},
    };

    static constexpr auto camPlayer2 = CameraTransform{
        .position = {-0.0478, 0.0341, 0.0556},
        .rotation = {-0.1269, -0.0244},
    };

    static constexpr auto camTV = CameraTransform{
        .position = {-0.0441, 0.1835, 0.1213},
        .rotation = {0.0566, -0.3779},
    };

    auto cutscene = ActionList{"test"_sh};
    const auto builder = actions::ActionListBuilder{
        .actionList = cutscene,
        .camera = camera,
        .dialogueBox = dialogueBox,
    };

    beginCutscene(cutscene);
    builder //
        .doFunc([this]() { game.songPlayer.restartMusic(); })
        .rotateTowards(npc, player)
        .say("Hello!", camNPC)
        .say("Hi...", camPlayer)
        .say("How's cutscene\ndevelopment\ngoing?", camNPC)
        .setCamera(camPlayer)
        .setAnimAndWait(player, "ThinkStart"_sh, ANGRY_FACE_ANIMATION)
        .say("Hmm...", player, "Think"_sh)
        .say("I think it's going...\n\3\4Pretty well\3\4\1...",
            player,
            {}, // don't change anim
            DEFAULT_BLINK_FACE_ANIMATION)
        .doFunc([this]() {
            game.songPlayer.pauseMusic();
            game.soundPlayer.playSound(22, game.newsSound);
        })
        .say("\2BREAKING NEWS!\1\nGleeby deeby\nhas escaped!", camTV)
        .doFunc([this]() {
            camera.setTransform(camPlayer);
            game.songPlayer.restartMusic();
        })
        .delay(2)
        .doFunc([this]() {
            game.songPlayer.pauseMusic();
            player.animator.pauseAnimation();
            player.setFaceAnimation(SHOCKED_FACE_ANIMATION);
        })
        .delay(2)
        .say("\3\2W-W-WHAT???\3\1", camNPC2, npc, "Shocked"_sh)
        .say("...", camPlayer)
        .doFunc([this]() {
            player.setFaceAnimation(DEFAULT_FACE_ANIMATION);
            player.animator.setAnimation("Idle"_sh);

            npc.animator.setAnimation("Idle"_sh);
        });
    endCutscene(cutscene);

    game.actionListManager.addActionList(eastl::move(cutscene));
}

void GameplayScene::beginCutscene(ActionList& list)
{
    list.addAction([this]() {
        // TODO: "stopMovement"
        player.animator.setAnimation("Idle"_sh);
        gameState = GameState::Dialogue;
        cutsceneBorderHeight = 0;
        cutsceneStart = true;
    });
}

void GameplayScene::endCutscene(ActionList& list, bool restoreOldCamera)
{
    if (restoreOldCamera) {
        oldTransformCutscene = CameraTransform{
            .position = camera.position,
            .rotation = camera.rotation,
        };
    }

    list.addAction([this, restoreOldCamera]() {
        if (restoreOldCamera) {
            camera.setTransform(oldTransformCutscene);
        }
        gameState = GameState::Normal;

        cutsceneStart = false;
    });
}

void GameplayScene::npcInteractSay(AnimatedModelObject& interactNPC, const eastl::string_view text)
{
    auto cutscene = ActionList{"interact"_sh};
    const auto builder = actions::ActionListBuilder{
        .actionList = cutscene,
        .camera = camera,
        .dialogueBox = dialogueBox,
    };

    auto t = CameraTransform{
        .position = {-0.1369, 0.0869, -1.0322},
        .rotation = {-0.0683, -0.8144},
    };

    auto fpos = CameraTransform{
        .rotation = {freeCameraRotPitch, -0.8222},
    };

    fpos.position = getCameraDesiredPosition(freeCameraRotPitch, fpos.rotation.y, false);

    const auto startNPCHeadRot = npc.model.armature.joints[4].localTransform.rotation;
    const auto targetNPCHeadRot = Quaternion{.w = 0.993, .x = 0.120, .y = 0.0, .z = 0.0};

    const auto startPlayerHeadRot = player.model.armature.joints[4].localTransform.rotation;
    const auto targetPlayerHeadRot = Quaternion{.w = 0.968, .x = -0.251, .y = 0.0, .z = 0.0};

    npc.useManualHeadRotation = true;
    player.useManualHeadRotation = true;

    static constexpr auto turnSpeed = psyqo::FixedPoint<>(3.0);
    static constexpr auto headTurnTime = psyqo::FixedPoint<>(0.5);

    // clang-format off
    beginCutscene(cutscene);
    builder //
        .parallelBegin()
            .moveCamera(t, 0.5)
            .rotateTowards(player, npc, turnSpeed)
            .rotateHeadTowards(npc, targetNPCHeadRot, headTurnTime)
            .rotateTowards(npc, player, turnSpeed)
            .rotateHeadTowards(player, targetPlayerHeadRot, headTurnTime)
        .parallelEnd()
        .say(text)
        .parallelBegin()
            .moveCamera(fpos, 0.5)
            .rotateHeadTowards(npc, targetNPCHeadRot, startNPCHeadRot, headTurnTime)
            .rotateHeadTowards(player, targetPlayerHeadRot, startPlayerHeadRot, headTurnTime)
        .parallelEnd();
    endCutscene(cutscene, false);
    // clang-format 

    game.actionListManager.addActionList(eastl::move(cutscene));
}
