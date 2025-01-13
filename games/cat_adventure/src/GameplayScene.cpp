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

#include <EASTL/bitset.h>

#include "Resources.h"

#define DEV_TOOLS

#define NEW_CAM

namespace
{
constexpr auto worldScale = 8.0;
consteval long double ToWorldCoords(long double d)
{
    return d / worldScale;
}

static constexpr auto TILE_SIZE = 8;

// Tiles which can be seen by the camera
// Stored in relative coordinates to the minimum point of frustum's AABB in XZ plane
static constexpr auto MAX_TILES_DIM = 32;
static eastl::bitset<MAX_TILES_DIM * MAX_TILES_DIM> tileSeen;

/* Adopted from https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/ */
int orient2d(int ax, int ay, int bx, int by, int cx, int cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

void rasterizeTriangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    int minX = eastl::min({x1, x2, x3});
    int maxX = eastl::max({x1, x2, x3});
    int minY = eastl::min({y1, y2, y3});
    int maxY = eastl::max({y1, y2, y3});

    minX = eastl::max(0, minX);
    maxX = eastl::min(MAX_TILES_DIM - 1, maxX);
    minY = eastl::max(0, minY);
    maxY = eastl::min(MAX_TILES_DIM - 1, maxY);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            int w0 = orient2d(x2, y2, x3, y3, x, y);
            int w1 = orient2d(x3, y3, x1, y1, x, y);
            int w2 = orient2d(x1, y1, x2, y2, x, y);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                tileSeen[y * MAX_TILES_DIM + x] = 1;
            }
        }
    }
}

} // end of anonymous namespace

GameplayScene::GameplayScene(Game& game) : game(game)
{}

void GameplayScene::start(StartReason reason)
{
    if (reason == StartReason::Create) {
        game.renderer.setFogNearFar(0.3, 1.5);
        // game.renderer.setFogNearFar(0.8, 16.125);
        static const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
        game.renderer.setFarColor(farColor);

        uiTexture = game.resourceCache.getResource<TextureInfo>(CATO_TEXTURE_HASH);

        interactionDialogueBox.displayBorders = false;
        interactionDialogueBox.textOffset.x = 48;
        interactionDialogueBox.position.y = 180;
        interactionDialogueBox.size.y = 32;
        interactionDialogueBox.displayMoreTextArrow = false;

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
        player.blinkTimer.reset(npc.blinkPeriod);

        player.jointGlobalTransforms.resize(player.model.armature.joints.size());
        player.animator.animations = &game.catAnimations;

        game.songPlayer.init(game.midi, game.vab);

        game.debugMenu.menuItems[DebugMenu::COLLISION_ITEM_ID].valuePtr = &collisionEnabled;
        game.debugMenu.menuItems[DebugMenu::FOLLOW_CAMERA_ITEM_ID].valuePtr = &followCamera;
        game.debugMenu.menuItems[DebugMenu::MUTE_MUSIC_ITEM_ID].valuePtr =
            &game.songPlayer.musicMuted;
        game.debugMenu.menuItems[DebugMenu::DRAW_COLLISION_ITEM_ID].valuePtr = &collisionDrawn;
    }

    triggers.clear();
    // staticObjects.clear();

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
    }

    // levelObj.setPosition({});
    // levelObj.rotation = {};

    if (game.level.id == 0) {
        game.renderer.setFogEnabled(false);

        /* levelObj.model =
            game.resourceCache.getResource<ModelData>(LEVEL1_MODEL_HASH).makeInstanceUnique();
         */

        player.setPosition({0.0, 0.0, 0.25});
        player.rotation = {0.0, -1.0};

        npc.model = game.resourceCache.getResource<ModelData>(HUMAN_MODEL_HASH).makeInstance();
        npc.jointGlobalTransforms.resize(npc.model.armature.joints.size());
        npc.animator.animations = &game.humanAnimations;
        npc.animator.setAnimation("Walk"_sh);

        npc.setPosition({0.0, 0.0, -0.11});
        npc.rotation = {0.0, 0.1};

        camera.position = {0.12, ToWorldCoords(1.5f), 0.81};
        camera.rotation = {0.0, 1.0};

        // debug
        camera.position = {0.2900, 0.4501, 0.5192};
        camera.rotation = {0.1621, -0.7753};
        // freeCamera = true;
        // followCamera = true;

        // camera.position = {0.1318, 0.1899, 0.1994};
        // camera.rotation = {0.0156, -0.8828};

        // player.setPosition({-0.2033, 0.0000, 0.0827});
        // player.setPosition({-0.5, 0.0, -0.11});
        followCamera = false;
    } else if (game.level.id == 1) {
        game.renderer.setFogEnabled(true);

        player.setPosition({0.5, 0.0, 0.5});
        player.rotation = {0.0, 1.37};

        camera.position = {0.f, ToWorldCoords(1.5f), -1.f};
        camera.rotation = {0.0, 0.0};

        followCamera = true;

        makeTestLevel();
    }

    canTalk = false;
    canInteract = false;
    player.animator.setAnimation("Idle"_sh);

    // tile cull test
    /*
    freeCamera = true;
    camera.position = {2.2587, 3.1545, 1.1418};
    camera.rotation = {0.3007, 1.3701};
    game.renderer.setFogNearFar(0.8, 50.125);
    debugInfoDrawn = true;
    */
}

void GameplayScene::frame()
{
    game.handleDeltas();

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
                gameState = GameState::Normal;
            }
        }
    }

    processDebugInput(pad);
}

void GameplayScene::processPlayerInput(const PadManager& pad)
{
    const auto& trig = game.trig;

    constexpr auto walkSpeed = psyqo::FixedPoint<>(0.25);
    constexpr auto sprintSpeed = psyqo::FixedPoint<>(0.6);
    constexpr auto rotateSpeed = psyqo::FixedPoint<>(1.0);
    const auto dt = game.frameDt;

    // yaw
    bool isRotating = false;
    if (pad.isButtonPressed(psyqo::SimplePad::Left)) {
        player.rotation.y += psyqo::FixedPoint<10, std::int32_t>(rotateSpeed * dt);
        isRotating = true;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Right)) {
        player.rotation.y -= psyqo::FixedPoint<10, std::int32_t>(rotateSpeed * dt);
        isRotating = true;
    }

    // go forward/backward
    bool isMoving = false;
    bool moveForward = false;
    if (pad.isButtonPressed(psyqo::SimplePad::Up)) {
        isMoving = true;
        moveForward = true;
    }

    if (pad.isButtonPressed(psyqo::SimplePad::Down)) {
        isMoving = true;
        moveForward = false;
    }

    bool isSprinting = false;
    if (pad.isButtonHeld(psyqo::SimplePad::Square)) {
        isSprinting = true;
    }

    player.velocity = {};

    if (isMoving) {
        psyqo::FixedPoint<> speed{};
        if (moveForward) {
            if (isSprinting) {
                speed = sprintSpeed;
                player.animator.setAnimation("Run"_sh, 1.0, 0.125);
            } else {
                speed = walkSpeed;
                player.animator.setAnimation("Walk"_sh, 1.0, 0.3);
            }
        } else {
            speed = -walkSpeed * 0.4;
            player.animator.setAnimation("Walk"_sh, -0.6, 0.3);
        }
        player.velocity = player.getFront() * speed;
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

    if (isMoving || isRotating) {
        if (player.animator.frameJustChanged()) {
            const auto animFrame = player.animator.getAnimationFrame();
            if (player.animator.getCurrentAnimationName() == "Walk"_sh) {
                if (animFrame == 3) {
                    game.soundPlayer.playSound(20, game.step1Sound);
                } else if (animFrame == 15) {
                    game.soundPlayer.playSound(21, game.step2Sound);
                }
            } else if (player.animator.getCurrentAnimationName() == "Run"_sh) {
                if (animFrame == 2) {
                    game.soundPlayer.playSound(20, game.step1Sound);
                } else if (animFrame == 10) {
                    game.soundPlayer.playSound(21, game.step2Sound);
                }
            }
        }
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Cross)) {
        if (canTalk) {
            gameState = GameState::Dialogue;
            player.animator.setAnimation("Idle"_sh);

            // TODO: move to npcInteractStart
            interactStartAngle = npc.rotation.y;
            interactEndAngle = npc.findInteractionAngle(player);
            interactRotationLerpFactor = 0.0;
            interactRotationLerpSpeed =
                math::calculateLerpDelta(interactStartAngle, interactEndAngle, 0.04);
            npcRotatesTowardsPlayer = true;
        } else if (canInteract) {
            gameState = GameState::Dialogue;
            player.animator.setAnimation("Idle"_sh);

            dialogueBox.setText("You see a \3\6gleeby-\ndeeby\1\3 on the\nscreen...");
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
        camera.rotation.y += psyqo::FixedPoint<10, std::int32_t>(rotateSpeed * dt);
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Right)) {
        camera.rotation.y -= psyqo::FixedPoint<10, std::int32_t>(rotateSpeed * dt);
    }

    // pitch
    if (pad.isButtonPressed(psyqo::SimplePad::L2)) {
        camera.rotation.x += psyqo::FixedPoint<10, std::int32_t>(rotateSpeed * dt * 0.6);
    }
    if (pad.isButtonPressed(psyqo::SimplePad::R2)) {
        camera.rotation.x -= psyqo::FixedPoint<10, std::int32_t>(rotateSpeed * dt * 0.6);
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

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Cross)) {
        fadeFinished = false;
        fadeLevel = 0;
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Triangle)) {
        player.setFaceAnimation(ANGRY_FACE_ANIMATION);
        player.animator.setAnimation("ThinkStart"_sh);
        cutscene = true;
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Circle)) {
        // switchLevel(1);
        fadeLevel++;
    }
}

void GameplayScene::updateCamera()
{
    const auto& trig = game.trig;

    if (!freeCamera && followCamera) {
#ifdef NEW_CAM
        static constexpr auto cameraOffset = psyqo::Vec3{
            .x = -0.1,
            .y = 0.42,
            .z = -0.60,
        };
        static constexpr auto cameraPitch = psyqo::FixedPoint<10>(0.085);
#else
        static constexpr auto cameraOffset = psyqo::Vec3{
            .x = -0.15,
            .y = 0.20,
            .z = -0.32,
        };
        static constexpr auto cameraPitch = psyqo::FixedPoint<10>(0.035);
#endif

        const auto fwdVector = player.getFront();
        const auto rightVector = player.getRight();

        const auto& playerPos = player.getPosition();
        camera.position.y = playerPos.y + cameraOffset.y;
        camera.position.x =
            playerPos.x + fwdVector.x * cameraOffset.z + rightVector.x * cameraOffset.x;
        camera.position.z =
            playerPos.z + fwdVector.z * cameraOffset.z + rightVector.z * cameraOffset.x;

        camera.rotation.x = cameraPitch;
        camera.rotation.y = player.rotation.y;
    }

    // calculate camera rotation matrix
    getRotationMatrix33RH(
        &camera.view.rotation, -camera.rotation.y, psyqo::SoftMath::Axis::Y, trig);
    psyqo::Matrix33 viewRotX;
    getRotationMatrix33RH(&viewRotX, -camera.rotation.x, psyqo::SoftMath::Axis::X, trig);

    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(viewRotX, camera.view.rotation, &camera.view.rotation);

    // PS1 has Y-down, so here's a fix for that
    // (basically, this is the same as doing 180-degree roll)
    camera.view.rotation.vs[0] = -camera.view.rotation.vs[0];
    camera.view.rotation.vs[1] = -camera.view.rotation.vs[1];

    camera.view.translation = -camera.position;
    // We do this on CPU because -camera.position can be outside of 4.12 fixed point range
    psyqo::SoftMath::
        matrixVecMul3(camera.view.rotation, camera.view.translation, &camera.view.translation);
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
        player.updateCollision();

        canInteract = false;
        if (collisionEnabled) {
            for (auto& trigger : triggers) {
                trigger.wasEntered = trigger.isEntered;
                if (trigger.interaction) {
                    trigger.isEntered = circleAABBIntersect(player.interactionCircle, trigger.aabb);
                } else {
                    trigger.isEntered = pointInAABB(trigger.aabb, player.getPosition());
                }

                if (trigger.wasJustEntered()) {
                    if (trigger.id == 0) {
                        switchLevel(1);
                    }
                }

                if (trigger.isEntered) {
                    canInteract = true;
                }
            }
        }
    }

    if (gameState == GameState::SwitchLevel) {
        updateLevelSwitch();
        if (startedLevelLoad) {
            return;
        }
    }

    player.update();
    if (game.level.id == 0) {
        npc.update();
    }

    updateCamera();
    calculateTileVisibility();

    if (gameState == GameState::Normal) {
        if (game.level.id == 0) {
            canTalk = circlesIntersect(player.interactionCircle, npc.interactionCircle);
        }
    } else if (gameState == GameState::Dialogue) {
        if (npcRotatesTowardsPlayer) {
            interactRotationLerpFactor += interactRotationLerpSpeed;
            if (interactRotationLerpFactor >= 1.0) { // finished rotation
                interactRotationLerpFactor = 1.0;
                npcRotatesTowardsPlayer = false;

                // TODO: generic interaction
                dialogueBox.setText("Hello!\nDialogues \2work\1!\n\3\4Amazing!");
            }

            npc.rotation.y =
                math::lerpAngle(interactStartAngle, interactEndAngle, interactRotationLerpFactor);
        }

        if (dialogueBox.isOpen) {
            dialogueBox.update();
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

    for (const auto& testCircle : collisionCircles) {
        if (circlesIntersect(player.collisionCircle, testCircle)) {
            anyCollision = true;
            goto collidedWithSomething;
        }
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

void GameplayScene::calculateTileVisibility()
{
    auto fov = psyqo::Angle(0.23);
    auto viewDistSide = psyqo::FixedPoint(3.5);

    // auto origin = player.getPosition();
    static psyqo::Vec3 origin;
    static auto yaw = camera.rotation.y;

    // ramsyscall_printf("yaw: %d\n", yaw.value);
    /* yaw.value = 1801;
    if (!freeCamera) {
        player.rotation.y.value = 1801;
    } */

#if 1

    // test
    // origin = {1.0905, 0.4199, 0.6447};

    // static psyqo::Angle testYaw = 1.3701;
    // testYaw += 0.005;
    // yaw = testYaw;

    // testYaw.value = 1678;

#endif
    const auto front = psyqo::Vec3{
        .x = game.trig.sin(camera.rotation.y),
        .y = 0.0,
        .z = game.trig.cos(camera.rotation.y),
    };
    if (!freeCamera || true) {
        origin = camera.position - front * 0.1;
        yaw = camera.rotation.y;
        // ramsyscall_printf("yaw: %d\n", yaw.value);
    }

    if (camera.rotation.x > 0.15) {
        // TODO: hande better - probably need to calculate
        origin = camera.position - front * 0.5;
        viewDistSide = psyqo::FixedPoint(4.0);
    }

    // yaw.value = 1924;
    // yaw.value = 1483;
    //  player.rotation.y = yaw;

    auto frustumLeft = origin + psyqo::Vec3{
                                    .x = viewDistSide * game.trig.sin(yaw - fov),
                                    .y = 0,
                                    .z = viewDistSide * game.trig.cos(yaw - fov)};
    auto frustumRight = origin + psyqo::Vec3{
                                     .x = viewDistSide * game.trig.sin(yaw + fov),
                                     .y = 0,
                                     .z = viewDistSide * game.trig.cos(yaw + fov)};

    const auto minX = eastl::min({origin.x, frustumLeft.x, frustumRight.x});
    const auto minZ = eastl::min({origin.z, frustumLeft.z, frustumRight.z});
    const auto maxX = eastl::max({origin.x, frustumLeft.x, frustumRight.x});
    const auto maxZ = eastl::max({origin.z, frustumLeft.z, frustumRight.z});

    minTileX = (minX * TILE_SIZE).floor();
    minTileZ = (minZ * TILE_SIZE).floor();
    maxTileX = (maxX * TILE_SIZE).ceil();
    maxTileZ = (maxZ * TILE_SIZE).ceil();

    const auto originTileX = (origin.x * TILE_SIZE).floor();
    const auto originTileZ = (origin.z * TILE_SIZE).floor();

    const auto pLeftTileX = (frustumLeft.x * TILE_SIZE).floor();
    const auto pLeftTiLeZ = (frustumLeft.z * TILE_SIZE).floor();

    const auto pRightTileX = (frustumRight.x * TILE_SIZE).floor();
    const auto pRightTileZ = (frustumRight.z * TILE_SIZE).floor();

    tileSeen.reset();

    rasterizeTriangle(
        maxTileZ - originTileZ,
        maxTileX - originTileX,
        maxTileZ - pLeftTiLeZ,
        maxTileX - pLeftTileX,
        maxTileZ - pRightTileZ,
        maxTileX - pRightTileX);
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

    // clear
    // psyqo::Color bg{{.r = 33, .g = 14, .b = 58}};
    psyqo::Color bg{{.r = 0, .g = 0, .b = 0}};
    auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
    gp.getNextClear(fill.primitive, bg);
    gp.chain(fill);

#if 0
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

    {
        auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
            psyqo::Prim::MaskControl::Set::ForceSet, psyqo::Prim::MaskControl::Test::No);
        gp.chain(maskBit);
    }

    {
        /* static constexpr auto fogColor = psyqo::Color{.r = 108, .g = 100, .b = 116};
        auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
        gp.getNextClear(fill.primitive, fogColor);
        gp.chain(fill); */

        static constexpr auto fogColor = psyqo::Color{.r = 108, .g = 100, .b = 116};

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

    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);

    numTilesDrawn = 0;
    {
        // auto playerPosX = (player.getPosition().x * 8).integer();
        // draw floor
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);
        psyqo::Vec3 v{};
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Translation>(v);
        for (int z = minTileZ; z <= maxTileZ; ++z) {
            for (int x = minTileX; x <= maxTileX; ++x) {
                // for (int z = -20; z < 20; ++z) {
                //    for (int x = -20 + playerPosX; x < 5 + playerPosX; ++x) {
                int tileId = (z == 0) ? 1 : 0;
                if (z == -1 || z == -2 || z == 1 || z == 2) {
                    tileId = 2;
                }
                // tileId = 3;

                const int xrel = maxTileX - x;
                const int zrel = maxTileZ - z;
                if (xrel >= 0 && xrel < MAX_TILES_DIM && zrel >= 0 && zrel < MAX_TILES_DIM &&
                    tileSeen[xrel * MAX_TILES_DIM + zrel] == 1) {
                    renderer.drawTileQuad(x, z, tileId, camera);
                    ++numTilesDrawn;
                }
            }
        }
    }
endTileRender:

    // renderer.bias = 300;
    // draw static objects
    for (auto& staticObject : game.level.staticObjects) {
        renderer.drawMeshObject(staticObject, camera);
    }

    gp.pumpCallbacks();

    renderer.bias = 0;
    // draw dynamic objects
    {
        // TODO: first draw objects without rotation
        // (won't have to upload camera.viewRot and change PseudoRegister::Rotation then)
        renderer.drawAnimatedModelObject(player, camera);
        if (game.level.id == 0) {
            renderer.drawAnimatedModelObject(npc, camera);
        }
    }

    gp.pumpCallbacks();

    gp.chain(ot);

    {
        /* static constexpr auto fogColor = psyqo::Color{.r = 108, .g = 100, .b = 116};
        auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
        gp.getNextClear(fill.primitive, fogColor);
        gp.chain(fill); */

        static constexpr auto fogColor = psyqo::Color{.r = 34, .g = 32, .b = 37};

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
        q.setSemiTrans();

        {
            auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
                psyqo::Prim::MaskControl::Set::ForceSet, psyqo::Prim::MaskControl::Test::Yes);
            gp.chain(maskBit);
        }
        gp.chain(quadFrag);
    }

    if (fadeLevel != 0) {
        auto& gpu = renderer.getGPU();

        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr.set(psyqo::Prim::TPageAttr::SemiTrans::FullBackSubFullFront);
        gpu.chain(tpage);

#if 0
        for (int i = 0; i < 4; ++i) {
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
#else
        auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
        auto& rect = rectFrag.primitive;
        rect.position = {};
        rect.size.x = SCREEN_WIDTH;
        rect.size.y = SCREEN_HEIGHT;

        const auto black =
            psyqo::Color{{uint8_t(fadeLevel), uint8_t(fadeLevel), uint8_t(fadeLevel)}};
        rect.setColor(black);
        rect.setSemiTrans();
        gpu.chain(rectFrag);
#endif
    }

    {
        auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
            psyqo::Prim::MaskControl::Set::FromSource, psyqo::Prim::MaskControl::Test::No);
        gp.chain(maskBit);
    }

#if 0
    if (debugInfoDrawn) {
        const auto front = psyqo::Vec3{
            .x = game.trig.sin(camera.rotation.y),
            .y = 0.0,
            .z = game.trig.cos(camera.rotation.y),
        };
        // auto front = player.getFront();
        /* renderer.drawLineWorldSpace(
            camera, origin, origin + front * 1.0, psyqo::Color{.r = 255, .g = 0, .b = 255}); */

        renderer.drawLineWorldSpace(
            camera, origin, frustumLeft, psyqo::Color{.r = 255, .g = 0, .b = 255});

        renderer.drawLineWorldSpace(
            camera, origin, frustumRight, psyqo::Color{.r = 255, .g = 0, .b = 255});

        renderer.drawAABB(
            camera,
            AABB{
                .min = {.x = minX, .y = 0, .z = minZ},
                .max = {.x = maxX, .y = 0, .z = maxZ},
            },
            psyqo::Color{.r = 0, .g = 255, .b = 0});
    }
#endif

    if (gameState == GameState::Normal) {
        if (canTalk) {
            interactionDialogueBox.setText("\5(X)\1 Talk", true);
            interactionDialogueBox.draw(renderer, game.font, game.fontTexture, uiTexture);
        } else if (canInteract) {
            interactionDialogueBox.setText("\5(X)\1 Interact", true);
            interactionDialogueBox.draw(renderer, game.font, game.fontTexture, uiTexture);
        }
    }

    if (gameState == GameState::Dialogue && dialogueBox.isOpen) {
        dialogueBox.draw(renderer, game.font, game.fontTexture, uiTexture);
    }

    /* for (int y = -10; y < 10; ++y) {
        for (int x = -10; x < 10; ++x) {
            AABB aabb{.min = {0, 0, 0}, .max = {0.125, 0, 0.125}};
            aabb.min.x += psyqo::FixedPoint<>(x, 0) * 0.125;
            aabb.min.z += psyqo::FixedPoint<>(y, 0) * 0.125;
            aabb.max.x += psyqo::FixedPoint<>(x, 0) * 0.125;
            aabb.max.z += psyqo::FixedPoint<>(y, 0) * 0.125;
            psyqo::Color c{.r = 255, .g = 0, .b = 255};
            renderer.drawAABB(camera, aabb, c);
        }
    } */

    if (debugInfoDrawn) {
        drawDebugInfo(renderer);
    }

    game.debugMenu.draw(renderer);
}

void GameplayScene::makeTestLevel()
{
#if 0
    MeshObject object;
    auto& levelModel = levelObj.model;
    auto& meshA = eastl::get<Mesh>(levelModel.meshes[0]);
    auto& meshB = eastl::get<Mesh>(levelModel.meshes[1]);

    for (int x = 0; x < 10; ++x) {
        for (int z = -3; z < 3; ++z) {
            if (z == 0) {
                object.mesh = meshA;
            } else {
                object.mesh = meshB;
            }
            object.setPosition(psyqo::FixedPoint(x, 0), 0.0, psyqo::FixedPoint(z, 0));
            object.calculateWorldMatrix();
            staticObjects.push_back(object);
        }
    }

    auto& tree = eastl::get<Mesh>(levelModel.meshes[4]);
    object.mesh = tree;
    for (int i = 0; i < 10; ++i) {
        object.setPosition(psyqo::FixedPoint(i, 0), 0.0, ToWorldCoords(8.0));
        object.calculateWorldMatrix();
        staticObjects.push_back(object);
    }

    auto& lamp = eastl::get<Mesh>(levelModel.meshes[2]);
    for (int i = 0; i < 10; ++i) {
        object.mesh = lamp;
        object.setPosition(psyqo::FixedPoint(i, 0), 0.0, ToWorldCoords(-8.0));
        object.calculateWorldMatrix();
        staticObjects.push_back(object);
    }

    object.mesh = eastl::get<Mesh>(levelModel.meshes[5]);
    object.setPosition(ToWorldCoords(8.0), 0.0, 0.0);
    object.calculateWorldMatrix();
    staticObjects.push_back(object);
#endif
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

        for (const auto& circle : collisionCircles) {
            renderer.drawCircle(camera, circle, colliderColor);
        }

        for (const auto& trigger : triggers) {
            const auto& col = trigger.isEntered ?
                                  activeTriggerColor :
                                  ((trigger.interaction) ? interactionTriggerColor : triggerColor);
            renderer.drawAABB(camera, trigger.aabb, col);
        }

        renderer
            .drawCircle(camera, player.collisionCircle, psyqo::Color{.r = 255, .g = 0, .b = 255});
        renderer.drawCircle(camera, npc.collisionCircle, psyqo::Color{.r = 0, .g = 255, .b = 255});

        if (canTalk || canInteract) {
            renderer.drawCircle(
                camera, player.interactionCircle, psyqo::Color{.r = 255, .g = 255, .b = 0});
        } else {
            renderer.drawCircle(
                camera, player.interactionCircle, psyqo::Color{.r = 255, .g = 0, .b = 0});
        }
    }

    /* {
        static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
        game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 32}},
            textCol,
            "pb used: %d, tiles drawn: %d",
            (int)renderer.getPrimBuffer().used(),
            numTilesDrawn);
    }
    return; */

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

        auto playerPosX = -(player.getPosition().x).integer();

        game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 16}},
            textCol,
            "p pos = (%.2f, %.2f, %.2f), %d",
            player.getPosition().x,
            player.getPosition().y,
            player.getPosition().z,
            playerPosX);

        game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 32}},
            textCol,
            "p rot=(%.2f, %.2f)",
            psyqo::FixedPoint<>(player.rotation.x),
            psyqo::FixedPoint<>(player.rotation.y));

        /* game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 64}},
            textCol,
            "%d, anim=%s",
            animIndex,
            player.animator.currentAnimation->name.getStr()); */

        /* game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 80}},
            textCol,
            "q = (%.3f, %.3f, %.3f, %.3f)",
            psyqo::FixedPoint<>(rot.w),
            psyqo::FixedPoint<>(rot.x),
            psyqo::FixedPoint<>(rot.y),
            psyqo::FixedPoint<>(rot.z)); */

        /* game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 80}},
            textCol,
            "t = (%.3f), f = %d",
            player.animator.normalizedAnimTime,
            player.animator.getAnimationFrame()); */

        game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 64}},
            textCol,
            "heap used: %d",
            (int)((uint8_t*)psyqo_heap_end() - (uint8_t*)psyqo_heap_start()));

        game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 80}},
            textCol,
            "pb used: %d, tiles drawn: %d",
            (int)renderer.getPrimBuffer().used(),
            numTilesDrawn);

        if (!freeCamera) {
            /* game.romFont.chainprintf(
                game.gpu(),
                {{.x = 16, .y = 64}},
                textCol,
                "bpm=%d, t=%d, reverb = %d",
                (int)game.songPlayer.bpm,
                (int)game.songPlayer.musicTime,
                (int)SoundPlayer::reverbEnabled); */
        }
        /* if (!freeCamera) {
            game.romFont.chainprintf(
                game.gpu(),
                {{.x = 16, .y = 64}},
                textCol,
                "%d, %d",
                game.levelModelFast.meshes[0].numTris,
                game.levelModelFast.meshes[0].numQuads);
        } */

        /* game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 64}},
            textCol,
            "%.2f",
            psyqo::FixedPoint<>(player.animator.normalizedAnimTime)); */

        game.romFont.chainprintf(
            game.gpu(),
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
    fsprintf(
        str,
        "camera.position = {%.4f, %.4f, %.4f};\ncamera.rotation = {%.4f, %.4f};",
        camera.position.x,
        camera.position.y,
        camera.position.z,
        psyqo::FixedPoint<>(camera.rotation.x),
        psyqo::FixedPoint<>(camera.rotation.y));
    ramsyscall_printf("%s\n", str.c_str());

    // dump player position/rotation
    fsprintf(
        str,
        "player.setPosition({%.4f, %.4f, %.4f});\nplayer.rotation = {%.4f, %.4f};",
        player.transform.translation.x,
        player.transform.translation.y,
        player.transform.translation.z,
        psyqo::FixedPoint<>(player.rotation.x),
        psyqo::FixedPoint<>(player.rotation.y));
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
