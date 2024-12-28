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

#define DEV_TOOLS

namespace
{
constexpr auto worldScale = 8.0;
consteval long double ToWorldCoords(long double d)
{
    return d / worldScale;
}

} // end of anonymous namespace

GameplayScene::GameplayScene(Game& game) : game(game)
{}

void GameplayScene::start(StartReason reason)
{
    if (reason == StartReason::Create) {
        game.renderer.setFogNearFar(0.6, 3.125);
        static const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
        game.renderer.setFarColor(farColor);

        uiTexture = game.resourceCache.getResource<TextureInfo>(CATO_TEXTURE_HASH);

        interactionDialogueBox.displayBorders = false;
        interactionDialogueBox.textOffset.x = 48;
        interactionDialogueBox.position.y = 180;
        interactionDialogueBox.size.y = 32;
        interactionDialogueBox.displayMoreTextArrow = false;

        player.model = &game.resourceCache.getResource<Model>(HUMAN_MODEL_HASH);
        player.texture = game.resourceCache.getResource<TextureInfo>(CATO_TEXTURE_HASH);
        player.jointGlobalTransforms.resize(player.model->armature.joints.size());
        player.animator.animations = &game.animations;

        game.songPlayer.init(game.midi, game.vab);

        game.debugMenu.menuItems[DebugMenu::COLLISION_ITEM_ID].valuePtr = &collisionEnabled;
        game.debugMenu.menuItems[DebugMenu::FOLLOW_CAMERA_ITEM_ID].valuePtr = &followCamera;
        game.debugMenu.menuItems[DebugMenu::MUTE_MUSIC_ITEM_ID].valuePtr =
            &game.songPlayer.musicMuted;
        game.debugMenu.menuItems[DebugMenu::DRAW_COLLISION_ITEM_ID].valuePtr = &collisionDrawn;
    }

    collisionBoxes.clear();
    triggers.clear();

    if (game.level.id == 0) { // TODO: load from level
        AABB collisionBox;

        collisionBox.min = {-0.34, 0.0, -0.32};
        collisionBox.max = {-0.1, 0.1, -0.0};
        collisionBoxes.push_back(collisionBox);

        collisionBox.min = {-0.34, 0.0, -0.32};
        collisionBox.max = {-0.16, 0.1, 0.34};
        collisionBoxes.push_back(collisionBox);

        collisionBox.min = {-0.34, 0.0, -0.32};
        collisionBox.max = {0.22, 0.1, -0.2};
        collisionBoxes.push_back(collisionBox);

        collisionBox.min = {0.08, 0.0, -0.32};
        collisionBox.max = {0.22, 0.1, 0.34};
        collisionBoxes.push_back(collisionBox);

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

    levelObj.setPosition({});
    levelObj.rotation = {};

    if (game.level.id == 0) {
        levelObj.texture = game.resourceCache.getResource<TextureInfo>(BRICKS_TEXTURE_HASH);

        game.renderer.setFogEnabled(false);

        levelObj.model = &game.resourceCache.getResource<Model>(LEVEL1_MODEL_HASH);

        player.setPosition({0.0, 0.0, 0.25});
        player.rotation = {0.0, -1.0};

        npc.model = &game.resourceCache.getResource<Model>(CATO_MODEL_HASH);
        npc.texture = game.resourceCache.getResource<TextureInfo>(CATO_TEXTURE_HASH);
        npc.jointGlobalTransforms.resize(npc.model->armature.joints.size());
        npc.animator.animations = &game.animations;

        npc.setPosition({0.0, 0.0, -0.11});
        npc.rotation = {0.0, 0.1};
        npc.animator.setAnimation("Idle"_sh);

        camera.position = {0.12, ToWorldCoords(1.5f), 0.81};
        camera.rotation = {0.0, 1.0};

        // debug
        camera.position = {0.2900, 0.4501, 0.5192};
        camera.rotation = {0.1621, -0.7753};
        // freeCamera = true;
        // followCamera = true;

        // player.setPosition({-0.2033, 0.0000, 0.0827});
        // player.setPosition({-0.5, 0.0, -0.11});
        followCamera = false;
    } else if (game.level.id == 1) {
        game.renderer.setFogEnabled(true);

        levelObj.model = &game.resourceCache.getResource<Model>(LEVEL2_MODEL_HASH);

        player.setPosition({0.5, 0.0, 0.5});
        player.rotation = {0.0, 0.5};

        camera.position = {0.f, ToWorldCoords(1.5f), -1.f};
        camera.rotation = {0.0, 0.0};

        followCamera = true;
    }

    canTalk = false;
    canInteract = false;
    player.animator.setAnimation("Idle"_sh);
}

void GameplayScene::frame()
{
    fpsCounter.update(game.gpu());
    game.pad.update();
    processInput(game.pad);
    update();

    gpu().pumpCallbacks();

    draw(game.renderer);
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

    constexpr auto walkSpeed = 0.0065;
    constexpr auto sprintSpeed = 0.02;
    constexpr auto rotateSpeed = 0.03;

    // yaw
    bool isRotating = false;
    if (pad.isButtonPressed(psyqo::SimplePad::Left)) {
        player.rotation.y += rotateSpeed;
        isRotating = true;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Right)) {
        player.rotation.y -= rotateSpeed;
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

    auto playerPos = player.getPosition();
    player.velocity = {};

    if (isMoving) {
        psyqo::FixedPoint<> speed{};
        if (moveForward) {
            if (isSprinting) {
                speed = sprintSpeed;
                player.animator.setAnimation("Run"_sh, 0.05, 0.125);
            } else {
                speed = walkSpeed;
                player.animator.setAnimation("Walk"_sh, 0.035, 0.3);
            }
        } else {
            speed = -walkSpeed * 0.4;
            player.animator.setAnimation("Walk"_sh, -0.025, 0.3);
        }
        player.velocity = player.getFront() * speed;
    } else if (isRotating) {
        player.animator.setAnimation("Walk"_sh, 0.025, 0.3);
    } else {
        player.animator.setAnimation("Idle"_sh);
    }

    if (isMoving) {
        if (player.animator.frameJustChanged()) {
            const auto animFrame = player.animator.getAnimationFrame();
            if (player.animator.currentAnimation->name == "Walk"_sh) {
                if (animFrame == 4) {
                    game.soundPlayer.playSound(20, game.step1Sound);
                } else if (animFrame == 20) {
                    game.soundPlayer.playSound(21, game.step2Sound);
                }
            } else if (player.animator.currentAnimation->name == "Run"_sh) {
                if (animFrame == 3) {
                    game.soundPlayer.playSound(20, game.step1Sound);
                } else if (animFrame == 15) {
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

    constexpr auto walkSpeed = 0.02;
    constexpr auto rotateSpeed = 0.01;

    // yaw
    if (pad.isButtonPressed(psyqo::SimplePad::Left)) {
        camera.rotation.y += rotateSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Right)) {
        camera.rotation.y -= rotateSpeed;
    }

    // pitch
    if (pad.isButtonPressed(psyqo::SimplePad::L2)) {
        camera.rotation.x += rotateSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::R2)) {
        camera.rotation.x -= rotateSpeed;
    }

    // go up/down
    if (pad.isButtonPressed(psyqo::SimplePad::L1)) {
        camera.position.y -= walkSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::R1)) {
        camera.position.y += walkSpeed;
    }

    // go forward/backward
    if (pad.isButtonPressed(psyqo::SimplePad::Up)) {
        camera.position.x += trig.sin(camera.rotation.y) * walkSpeed;
        camera.position.z += trig.cos(camera.rotation.y) * walkSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Down)) {
        camera.position.x -= trig.sin(camera.rotation.y) * walkSpeed;
        camera.position.z -= trig.cos(camera.rotation.y) * walkSpeed;
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
}

void GameplayScene::updateCamera()
{
    const auto& trig = game.trig;

    if (!freeCamera && followCamera) {
        static constexpr auto cameraOffset = psyqo::Vec3{
            .x = -0.05,
            .y = 0.21,
            .z = -0.26,
        };
        static constexpr auto cameraPitch = psyqo::FixedPoint<10>(0.045);

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
        pos += player.velocity;
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
    }

    player.update();
    if (game.level.id == 0) {
        npc.update();
    }

    updateCamera();

    if (gameState == GameState::Normal) {
        canTalk = circlesIntersect(player.interactionCircle, npc.interactionCircle);
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
        game.loadLevel(destinationLevelId);

        fadeFinished = false;
        fadeOut = false;
        fadeLevel = 255;
        switchLevelState = SwitchLevelState::FadeIn;
        break;
    case SwitchLevelState::FadeIn:
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
        newPos.x += player.velocity.x;
    } else if (axis == psyqo::SoftMath::Axis::Z) {
        newPos.z += player.velocity.z;
    }
    player.setPosition(newPos);
    player.updateCollision();

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

    // clear
    psyqo::Color bg{{.r = 33, .g = 14, .b = 58}};
    // psyqo::Color bg{{.r = 0, .g = 0, .b = 0}};
    auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
    gp.getNextClear(fill.primitive, bg);
    gp.chain(fill);

    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);

    // draw static objects
    if (game.level.id == 0) {
        renderer.drawModelObject(levelObj, camera, false);
    } else if (game.level.id == 1) {
        drawTestLevel(renderer);
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

    if (debugInfoDrawn) {
        drawDebugInfo(renderer);
    }

    if (gameState == GameState::SwitchLevel) {
        auto& primBuffer = renderer.getPrimBuffer();
        auto& gpu = renderer.getGPU();

        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr.set(psyqo::Prim::TPageAttr::SemiTrans::FullBackSubFullFront);
        gpu.chain(tpage);

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
    }

    game.debugMenu.draw(renderer);
}

void GameplayScene::drawTestLevel(Renderer& renderer)
{
    MeshObject object;
    const auto& levelModel = *levelObj.model;
    auto meshA = &levelModel.meshes[0];
    auto meshB = &levelModel.meshes[1];

    renderer.bias = 1000;
    for (int x = 0; x < 10; ++x) {
        for (int z = -3; z < 3; ++z) {
            if (z == 0) {
                object.mesh = meshA;
            } else {
                object.mesh = meshB;
            }
            object.setPosition(psyqo::FixedPoint(x, 0), 0.0, psyqo::FixedPoint(z, 0));
            object.calculateWorldMatrix();
            renderer.drawMeshObject(object, camera);
        }
    }

    auto tree = &levelModel.meshes[4];
    object.mesh = tree;
    for (int i = 0; i < 10; ++i) {
        object.setPosition(psyqo::FixedPoint(i, 0), 0.0, ToWorldCoords(8.0));
        object.calculateWorldMatrix();
        renderer.drawMeshObject(object, camera);
    }

    auto lamp = &levelModel.meshes[2];
    object.mesh = lamp;
    for (int i = 0; i < 10; ++i) {
        object.setPosition(psyqo::FixedPoint(i, 0), 0.0, ToWorldCoords(-8.0));
        object.calculateWorldMatrix();
        renderer.drawMeshObject(object, camera);
    }

    auto car = &levelModel.meshes[5];
    renderer.bias = -100;
    object.mesh = car;
    object.setPosition(ToWorldCoords(8.0), 0.0, 0.0);
    object.calculateWorldMatrix();
    renderer.drawMeshObject(object, camera);
}

void GameplayScene::drawDebugInfo(Renderer& renderer)
{
    if (collisionDrawn) { // draw collisions/triggers
        renderer.drawObjectAxes(player, camera);
        // player.model->armature.drawDebug(renderer);
        renderer.drawArmature(npc, camera);

        static const auto colliderColor = psyqo::Color{.r = 128, .g = 255, .b = 255};
        static const auto triggerColor = psyqo::Color{.r = 255, .g = 255, .b = 128};
        static const auto interactionTriggerColor = psyqo::Color{.r = 255, .g = 128, .b = 128};
        static const auto activeTriggerColor = psyqo::Color{.r = 128, .g = 255, .b = 128};

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

        game.romFont.chainprintf(
            game.gpu(),
            {{.x = 16, .y = 16}},
            textCol,
            "p pos = (%.2f, %.2f, %.2f), st:%s, l:%d",
            player.getPosition().x,
            player.getPosition().y,
            player.getPosition().z,
            stateStr,
            game.level.id);

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
            "bpm=%d, t=%d, reverb = %d",
            (int)game.songPlayer.bpm,
            (int)game.songPlayer.musicTime,
            (int)SoundPlayer::reverbEnabled); */

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
            {{.x = 16, .y = 48}},
            textCol,
            "FPS: %.2f, avg: %.2f",
            fpsCounter.getMovingAverage(),
            fpsCounter.getAverage());
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
