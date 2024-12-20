#include "GameplayScene.h"

#include <psyqo/gte-registers.hh>
#include <psyqo/simplepad.hh>
#include <psyqo/soft-math.hh>

#include <psyqo/primitives/lines.hh>
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/primitives/sprites.hh>

#include "Common.h"
#include "Game.h"
#include "Renderer.h"

#include "gte-math.h"

#include <common/syscalls/syscalls.h>

#include <psyqo/xprintf.h>

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
    game.renderer.setFogNearFar(0.6, 3.125);
    static const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
    game.renderer.setFarColor(farColor);

    if (reason == StartReason::Create) {
        player.model = &game.catoModel;
        playerAnimator.animations = &game.animations;
        playerAnimator.setAnimation("Idle"_sh);

        npc.model = &game.catoModel2;
        npcAnimator.animations = &game.animations;
        npcAnimator.setAnimation("Idle"_sh);

        car.model = &game.carModel;
        levelObj.model = &game.levelModel;
        levelObj.setPosition({});
        levelObj.rotation = {};

        if (game.levelId == 0) {
            player.setPosition({0.0, 0.0, 0.25});
            player.rotation = {0.0, -1.0};

            npc.setPosition({0.0, 0.0, 0.1});
            npc.rotation = {0.0, 0.1};

            camera.position = {0.12, ToWorldCoords(1.5f), 0.81};
            camera.rotation = {0.0, 1.0};

            camera.rotation.x = 0.05;
        } else if (game.levelId == 1) {
            player.setPosition({0.5, 0.0, 0.5});
            player.rotation = {0.0, 1.0};

            car.setPosition({0.0, 0.0, 0.5});
            car.rotation = {0.0, 0.2};

            camera.position = {0.f, ToWorldCoords(1.5f), -1.f};
            camera.rotation = {0.0, 0.0};
        }
    }

    game.songPlayer.init(game.midi, game.vab);
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
    if (freeCamera) {
        processFreeCameraInput(pad);
    } else {
        processPlayerInput(pad);
    }

    dialogueBox.handleInput(pad);

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
    if (isMoving) {
        if (moveForward) {
            if (isSprinting) {
                playerPos.x += trig.sin(player.rotation.y) * sprintSpeed;
                playerPos.z += trig.cos(player.rotation.y) * sprintSpeed;
                playerAnimator.setAnimation("Run"_sh, 0.05, 0.125);
            } else {
                playerPos.x += trig.sin(player.rotation.y) * walkSpeed;
                playerPos.z += trig.cos(player.rotation.y) * walkSpeed;
                playerAnimator.setAnimation("Walk"_sh, 0.035, 0.3);
            }
        } else {
            playerPos.x -= trig.sin(player.rotation.y) * walkSpeed * 0.4;
            playerPos.z -= trig.cos(player.rotation.y) * walkSpeed * 0.4;
            playerAnimator.setAnimation("Walk"_sh, -0.025, 0.3);
        }
        player.setPosition(playerPos);
    } else if (isRotating) {
        playerAnimator.setAnimation("Walk"_sh, 0.025, 0.3);
    } else {
        playerAnimator.setAnimation("Idle"_sh);
    }

    if (isMoving) {
        if (playerAnimator.frameJustChanged()) {
            const auto animFrame = playerAnimator.getAnimationFrame();
            if (playerAnimator.currentAnimationName == "Walk"_sh) {
                if (animFrame == 4) {
                    game.soundPlayer.playSound(20, game.step1Sound);
                } else if (animFrame == 20) {
                    game.soundPlayer.playSound(21, game.step2Sound);
                }
            } else if (playerAnimator.currentAnimationName == "Run"_sh) {
                if (animFrame == 3) {
                    game.soundPlayer.playSound(20, game.step1Sound);
                } else if (animFrame == 15) {
                    game.soundPlayer.playSound(21, game.step2Sound);
                }
            }
        }
    }

    if (pad.isButtonPressed(psyqo::SimplePad::L2)) {
        camera.rotation.x += rotateSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::R2)) {
        camera.rotation.x -= rotateSpeed;
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

    if (freeCamera) {
        if (pad.wasButtonJustPressed(psyqo::SimplePad::Cross)) {
            ++animIndex;
            if (animIndex >= game.animations.size()) {
                animIndex = 0;
            }
            playerAnimator.setAnimation(game.animations[animIndex].name);
        }

        if (pad.wasButtonJustPressed(psyqo::SimplePad::Triangle)) {
            if (animIndex != 0) {
                --animIndex;
            } else {
                animIndex = game.animations.size() - 1;
            }
            playerAnimator.setAnimation(game.animations[animIndex].name);
        }
    }
}

void GameplayScene::updateCamera()
{
    const auto& trig = game.trig;

    if (!freeCamera) {
        static constexpr auto cameraOffset = psyqo::Vec3{
            .x = -0.05,
            .y = 0.21,
            .z = -0.16,
        };
        static constexpr auto cameraPitch = psyqo::FixedPoint<10>(0.045);

        const auto fwdVector = player.getFront(trig);
        const auto rightVector = player.getRight(trig);

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
    getRotationMatrix33RH(&camera.viewRot, -camera.rotation.y, psyqo::SoftMath::Axis::Y, trig);
    psyqo::Matrix33 viewRotX;
    getRotationMatrix33RH(&viewRotX, -camera.rotation.x, psyqo::SoftMath::Axis::X, trig);

    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(viewRotX, camera.viewRot, &camera.viewRot);

    // PS1 has Y-down, so here's a fix for that
    // (basically, this is the same as doing 180-degree roll)
    camera.viewRot.vs[0] = -camera.viewRot.vs[0];
    camera.viewRot.vs[1] = -camera.viewRot.vs[1];
}

void GameplayScene::update()
{
    updateCamera();

    // spin the cat
    // cato.rotation.y += 0.01;
    // cato.rotation.x = 0.25;

    playerAnimator.update();
    playerAnimator.animate(*player.model);

    npcAnimator.update();
    npcAnimator.animate(*npc.model);

    player.calculateWorldMatrix();
    car.calculateWorldMatrix();
    npc.calculateWorldMatrix();

    dialogueBox.update();
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

    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);

    // draw static objects
    if (game.levelId == 0) {
        renderer.drawModelObject(levelObj, camera, game.bricksTexture, false);
    } else if (game.levelId == 1) {
        drawTestLevel(renderer);
    }

    gp.pumpCallbacks();

    renderer.bias = 0;
    // draw dynamic objects
    {
        // TODO: first draw objects without rotation
        // (won't have to upload camera.viewRot and change PseudoRegister::Rotation then)

        {
            renderer.drawModelObject(player, camera, game.catoTexture);
        }

        {
            renderer.drawModelObject(npc, camera, game.catoTexture);
        }

        if (game.levelId == 1) {
            renderer.drawModelObject(car, camera, game.carTexture);
        }
    }

    gp.pumpCallbacks();

    gp.chain(ot);

    // dialogueBox.draw(renderer, game.font, game.fontTexture, game.catoTexture);

    if (debugInfoDrawn) {
        drawDebugInfo(renderer);
    }
}

void GameplayScene::drawTestLevel(Renderer& renderer)
{
    MeshObject object;
    auto meshA = &game.levelModel.meshes[0];
    auto meshB = &game.levelModel.meshes[1];
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
            renderer.drawMeshObject(object, camera, game.catoTexture);
        }
    }

    auto tree = &game.levelModel.meshes[4];
    object.mesh = tree;
    for (int i = 0; i < 10; ++i) {
        object.setPosition(psyqo::FixedPoint(i, 0), 0.0, ToWorldCoords(8.0));
        object.calculateWorldMatrix();
        renderer.drawMeshObject(object, camera, game.catoTexture);
    }

    auto lamp = &game.levelModel.meshes[2];
    object.mesh = lamp;
    for (int i = 0; i < 10; ++i) {
        object.setPosition(psyqo::FixedPoint(i, 0), 0.0, ToWorldCoords(-8.0));
        object.calculateWorldMatrix();
        renderer.drawMeshObject(object, camera, game.catoTexture);
    }

    auto car = &game.levelModel.meshes[5];
    renderer.bias = -100;
    object.mesh = car;
    object.setPosition(ToWorldCoords(8.0), 0.0, 0.0);
    object.calculateWorldMatrix();
    renderer.drawMeshObject(object, camera, game.catoTexture);
}

void GameplayScene::drawDebugInfo(Renderer& renderer)
{
    renderer.drawObjectAxes(player, camera);

    /* renderer.drawLineLocalSpace(
        {0, ToWorldCoords(1.31), 0},
        {0, ToWorldCoords(1.31 + 0.3), 0},
        {.r = 255, .g = 255, .b = 0}); */

    auto& armature = game.catoModel.armature;
    armature.drawDebug(renderer);

    /* static eastl::fixed_string<char, 512> str;
    auto test = psyqo::Vec4{1.0, 2.0, 3.0, 4.0};
    fsprintf(str, "%.2f, %.2f, %.2f, %.2f\n", test.x, test.y, test.z, test.w);
    ramsyscall_printf("vec = %s\n", str.c_str()); */

    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        textCol,
        "cam pos = (%.2f, %.2f, %.2f)",
        player.transform.translation.x,
        player.transform.translation.y,
        player.transform.translation.z);

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 32}},
        textCol,
        "camera rot=(%.2f, %.2f)",
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

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 64}},
        textCol,
        "%d, anim=%s",
        animIndex,
        playerAnimator.currentAnimation->name.getStr());

    auto& rot = armature.joints[armature.selectedJoint].localTransform.rotation;

    /* game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 80}},
        textCol,
        "q = (%.3f, %.3f, %.3f, %.3f)",
        psyqo::FixedPoint<>(rot.w),
        psyqo::FixedPoint<>(rot.x),
        psyqo::FixedPoint<>(rot.y),
        psyqo::FixedPoint<>(rot.z)); */

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 80}},
        textCol,
        "t = (%.3f), f = %d",
        playerAnimator.normalizedAnimTime,
        playerAnimator.getAnimationFrame());

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 48}},
        textCol,
        "FPS: %.2f, avg: %.2f",
        fpsCounter.getMovingAverage(),
        fpsCounter.getAverage());
}
