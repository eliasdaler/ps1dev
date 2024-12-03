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
    return d / 8.0;
}

} // end of anonymous namespace

GameplayScene::GameplayScene(Game& game, Renderer& renderer) : game(game), renderer(renderer)
{}

void GameplayScene::start(StartReason reason)
{
    // screen "center" (screenWidth / 2, screenHeight / 2)
    psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_WIDTH / 2.0).raw());
    psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_HEIGHT / 2.0).raw());

    // projection plane distance
    psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(300);

    // FIXME: use OT_SIZE here somehow?
    psyqo::GTE::write<psyqo::GTE::Register::ZSF3, psyqo::GTE::Unsafe>(1024 / 3);
    psyqo::GTE::write<psyqo::GTE::Register::ZSF4, psyqo::GTE::Unsafe>(1024 / 4);

    renderer.setFogNearFar(2500, 12800, SCREEN_WIDTH / 2);
    // far color
    const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
    psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(farColor.r);
    psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(farColor.g);
    psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Unsafe>(farColor.b);

    if (reason == StartReason::Create) {
        cato.model = &game.catoModel;
        car.model = &game.carModel;

        if (game.levelId == 0) {
            cato.position = {0.0, 0.0, 0.0};
            cato.rotation = {0.0, 0.0};

            camera.position = {0.12, ToWorldCoords(1.5f), 0.81};
            camera.rotation = {0.0, 1.0};

            // skeleton debug
            camera.position = {-0.13, 0.10, 0.24};
            camera.rotation = {0.0, 0.83};

            // car.position = {0.0, 0.0, 5.0};
        } else if (game.levelId == 1) {
            cato.position = {0.5, 0.0, 0.5};
            cato.rotation = {0.0, 1.0};

            car.position = {0.0, 0.0, 0.5};
            car.rotation = {0.0, 0.2};

            camera.position = {0.f, ToWorldCoords(1.5f), -1.f};
            camera.rotation = {0.0, 0.0};
        }
    }

    game.songPlayer.init(game.midi, game.vab);

    skeleton.makeTestSkeleton();

    startRotation = skeleton.getRootJoint().localTransform.rotation;
    targetRotation = {0, 0, 0, 1};

    slerpFactor = 0.0;
}

void GameplayScene::onResourcesLoaded()
{}

void GameplayScene::frame()
{
    const auto currentFrameCounter = gpu().getFrameCount();
    frameDiff = currentFrameCounter - lastFrameCounter;
    lastFrameCounter = currentFrameCounter;

    slerpFactor += 0.005;
    if (slerpFactor > 1.0) {
        slerpFactor = 0.0;
    }

    skeleton.getRootJoint().localTransform.rotation =
        slerp(startRotation, targetRotation, slerpFactor);

    skeleton.calculateTransforms();

    processInput();
    update();

    gpu().pumpCallbacks();

    draw();
}

void GameplayScene::processInput()
{
    const auto& pad = game.pad;
    const auto& trig = game.trig;

    constexpr auto walkSpeed = 0.02;
    constexpr auto rotateSpeed = 0.01;

    // yaw
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Left)) {
        camera.rotation.y += rotateSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Right)) {
        camera.rotation.y -= rotateSpeed;
    }

    // pitch
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::L2)) {
        camera.rotation.x += rotateSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::R2)) {
        camera.rotation.x -= rotateSpeed;
    }

    // go up/down
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::L1)) {
        camera.position.y -= walkSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::R1)) {
        camera.position.y += walkSpeed;
    }

    // go forward/backward
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Up)) {
        camera.position.x += trig.sin(camera.rotation.y) * walkSpeed;
        camera.position.z += trig.cos(camera.rotation.y) * walkSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Down)) {
        camera.position.x -= trig.sin(camera.rotation.y) * walkSpeed;
        camera.position.z -= trig.cos(camera.rotation.y) * walkSpeed;
    }

    static bool wasLeftPressed = false;
    if (!wasLeftPressed && pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Left)) {
        toneNum -= 1;

        reverbPreset -= 1;
        // game.soundPlayer.setReverbPreset((SpuReverbPreset)reverbPreset);

        wasLeftPressed = true;

        /* SoundPlayer::reverbEnabled = !SoundPlayer::reverbEnabled;
        if (!SoundPlayer::reverbEnabled) {
            game.soundPlayer.setReverbPreset(SpuReverbPreset::Off);
        } else {
            game.soundPlayer.setReverbPreset(SpuReverbPreset::StudioLarge);
        } */
    }
    if (!pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Left)) {
        wasLeftPressed = false;
    }

    static bool wasRightPressed = false;
    if (!wasRightPressed && pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Right)) {
        toneNum += 1;

        reverbPreset += 1;
        // game.soundPlayer.setReverbPreset((SpuReverbPreset)reverbPreset);

        wasRightPressed = true;
    }
    if (!pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Right)) {
        wasRightPressed = false;
    }

    dialogueBox.handleInput(game.pad);
}

void GameplayScene::updateCamera()
{
    // calculate camera rotation matrix
    camera.viewRot = psyqo::SoftMath::
        generateRotationMatrix33(camera.rotation.y, psyqo::SoftMath::Axis::Y, game.trig);
    const auto viewRotX = psyqo::SoftMath::
        generateRotationMatrix33(camera.rotation.x, psyqo::SoftMath::Axis::X, game.trig);

    // only done once per frame - ok to do on CPU
    psyqo::SoftMath::multiplyMatrix33(camera.viewRot, viewRotX, &camera.viewRot);
    /* psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(viewRotX, camera.viewRot, &camera.viewRot); */

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

    cato.calculateWorldMatrix();
    car.calculateWorldMatrix();

    dialogueBox.update();
}

void GameplayScene::draw()
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

    // draw static objects
    if (game.levelId == 0) {
        // verts are stored in world coords (limited to approximately (-8.f;8.f) range)
        auto posCamSpace = -camera.position;
        psyqo::SoftMath::matrixVecMul3(camera.viewRot, posCamSpace, &posCamSpace);

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

        renderer.drawModel(game.levelModel, game.bricksTexture);
    }

    if (game.levelId == 1) { // level WIP
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);

        MeshObject object;
        auto* meshA = &game.levelModel.meshes[0];
        auto* meshB = &game.levelModel.meshes[1];
        renderer.bias = 1000;
        for (int x = 0; x < 10; ++x) {
            for (int z = -3; z < 3; ++z) {
                if (z == 0) {
                    object.mesh = meshA;
                } else {
                    object.mesh = meshB;
                }
                object.position.x = psyqo::FixedPoint(x, 0);
                object.position.y = 0.0;
                object.position.z = psyqo::FixedPoint(z, 0);
                object.calculateWorldMatrix();
                renderer.drawMeshObject(object, camera, game.catoTexture);
            }
        }

        auto* tree = &game.levelModel.meshes[4];
        object.mesh = tree;
        for (int i = 0; i < 10; ++i) {
            object.position.x = psyqo::FixedPoint(i, 0);
            object.position.z = ToWorldCoords(8.0);
            object.calculateWorldMatrix();
            renderer.drawMeshObject(object, camera, game.catoTexture);
        }

        auto* lamp = &game.levelModel.meshes[2];
        object.mesh = lamp;
        for (int i = 0; i < 10; ++i) {
            object.position.x = psyqo::FixedPoint(i, 0);
            object.position.z = ToWorldCoords(-8.0);
            object.calculateWorldMatrix();
            renderer.drawMeshObject(object, camera, game.catoTexture);
        }

        auto* car = &game.levelModel.meshes[5];
        renderer.bias = -100;
        object.mesh = car;
        object.position = {};
        object.position.x = ToWorldCoords(8.0);
        object.calculateWorldMatrix();
        renderer.drawMeshObject(object, camera, game.catoTexture);
    }

    gp.pumpCallbacks();

    renderer.bias = 0;
    // draw dynamic objects
    {
        // TODO: first draw objects without rotation
        // (won't have to upload camera.viewRot and change PseudoRegister::Rotation then)

        {
            renderer.drawModelObject(cato, camera, game.catoTexture);
        }

        if (game.levelId == 1) {
            renderer.drawModelObject(car, camera, game.carTexture);
        }
    }

    gp.pumpCallbacks();

    gp.chain(ot);

    // dialogueBox.draw(renderer, game.font, game.fontTexture, game.catoTexture);

    drawDebugInfo();
}

void GameplayScene::drawDebugInfo()
{
    renderer.drawObjectAxes(cato, camera);

    /* renderer.drawLineLocalSpace(
        {0, ToWorldCoords(1.31), 0},
        {0, ToWorldCoords(1.31 + 0.3), 0},
        {.r = 255, .g = 255, .b = 0}); */

    skeleton.drawDebug(renderer);

    /* static eastl::fixed_string<char, 512> str;
    auto test = psyqo::Vec4{1.0, 2.0, 3.0, 4.0};
    fsprintf(str, "%.2f, %.2f, %.2f, %.2f\n", test.x, test.y, test.z, test.w);
    ramsyscall_printf("vec = %s\n", str.c_str()); */

    /* renderer.drawLineLocalSpace(
        {0, ToWorldCoords(0.85), 0}, {0, ToWorldCoords(0.85 + 0.3), 0}, {.r = 255, .g = 0, .b = 0});
     */

    /* { // draw some test lines in world space
        renderer.drawLineWorldSpace(
            camera, {0., 0., -0.4}, {0., 0.1, -0.4}, {.r = 255, .g = 255, .b = 255});
        renderer.drawLineWorldSpace(
            camera, {0., 0.1, -0.4}, {0.1, 0.12, -0.4}, {.r = 255, .g = 0, .b = 255});
        renderer.drawLineWorldSpace(
            camera, {0.1, 0.12, -0.4}, {0.2, 0.2, -0.2}, {.r = 0, .g = 255, .b = 255});
    } */

    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        textCol,
        "cam pos = (%.2f, %.2f, %.2f)",
        camera.position.x,
        camera.position.y,
        camera.position.z);

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 32}},
        textCol,
        "cam rot=(%.2a, %.2a)",
        camera.rotation.x,
        camera.rotation.y);

    /* game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 64}},
        textCol,
        "bpm=%d, t=%d, reverb = %d",
        (int)game.songPlayer.bpm,
        (int)game.songPlayer.musicTime,
        (int)SoundPlayer::reverbEnabled); */

    const auto fps = gpu().getRefreshRate() / frameDiff;
    fpsMovingAverageNew = alpha * fps + oneMinAlpha * fpsMovingAverageOld;
    fpsMovingAverageOld = fpsMovingAverageNew;

    newFPS.value = fps << 12;
    // lerp
    avgFPS = avgFPS + lerpFactor * (newFPS - avgFPS);

    /* game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 48}},
        textCol,
        "FPS: %.2f, avg: %.2f",
        fpsMovingAverageNew,
        avgFPS); */
}
