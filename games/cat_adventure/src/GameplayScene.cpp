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

namespace
{
void SetFogNearFar(int a, int b, int h)
{
    // TODO: check params + add asserts?
    const auto dqa = ((-a * b / (b - a)) << 8) / h;
    const auto dqaF = eastl::clamp(dqa, -32767, 32767);
    const auto dqbF = ((b << 12) / (b - a) << 12);

    psyqo::GTE::write<psyqo::GTE::Register::DQA, psyqo::GTE::Unsafe>(dqaF);
    psyqo::GTE::write<psyqo::GTE::Register::DQB, psyqo::GTE::Safe>(dqbF);
}
}

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

    SetFogNearFar(1500, 12800, SCREEN_WIDTH / 2);
    // far color
    const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
    psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(farColor.r);
    psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(farColor.g);
    psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Unsafe>(farColor.b);

    if (reason == StartReason::Create) {
        cato.model = &game.catoModel;
        car.model = &game.carModel;

        if (game.levelId == 0) {
            cato.position = {0.0, 0.22, -0.4};
            cato.rotation = {0.0, 0.2};
            camera.position = {0.59, 0, -0.84};
            camera.rotation = {0.f, -0.25f};

            car.position = {0.0, 0.0, 5.0};

        } else if (game.levelId == 1) {
            cato.position = {0.0, 0.0, 0.0};

            car.position = {0.0, 0.0, 0.5};
            car.rotation = {0.0, 0.2};

            camera.position = {0.f, -0.25f, -1.f};
            camera.rotation = {0.0, 0.0};
        }
    }

    for (int i = 0; i < lastEvent.size(); ++i) {
        lastEvent[i] = 0;
    }

    for (int i = 0; i < eventNum.size(); ++i) {
        eventNum[i] = 0;
    }

    for (int i = 0; i < eventNum.size(); ++i) {
        eventNum[i] = 0;
        channelUsers[i] = {};
    }

    musicTimer = gpu().armPeriodicTimer(20000, [this](uint32_t) {
        musicTime += 20;
        updateMusic();
    });
}

void GameplayScene::onResourcesLoaded()
{}

void GameplayScene::frame()
{
    const auto currentFrameCounter = gpu().getFrameCount();
    frameDiff = currentFrameCounter - lastFrameCounter;
    lastFrameCounter = currentFrameCounter;

    processInput();
    update();

    gpu().pumpCallbacks();

    draw();

    gpu().pumpCallbacks();
}

void GameplayScene::processInput()
{
    const auto& pad = game.pad;
    const auto& trig = game.trig;

    constexpr auto walkSpeed = 0.02;
    // constexpr auto walkSpeed = 2.0;
    constexpr auto rotateSpeed = 0.01;

    // yaw
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Left)) {
        camera.rotation.y -= rotateSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Right)) {
        camera.rotation.y += rotateSpeed;
    }

    // pitch
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::L2)) {
        camera.rotation.x -= rotateSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::R2)) {
        camera.rotation.x += rotateSpeed;
    }

    // go up/down
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::L1)) {
        camera.position.y += walkSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::R1)) {
        camera.position.y -= walkSpeed;
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

    ++count;
    if (count > 10) {
        count = 10;
    }

    if (count == 10) {
        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Cross)) {
            pitchBase = -1;
            reset = 1;
            count = 0;
        }

        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Square)) {
            pitchBase = 1;
            reset = 1;
            count = 0;
        }

        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Triangle)) {
            pitchBase = 0;
            reset = 1;
            count = 0;
        }

        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Circle)) {
            pitchBase = -2;
            reset = 1;
            count = 0;
        }
    }

    auto& soundPlayer = game.soundPlayer;

    if (reset && count == 0) {
        reset = 0;
        psyqo::FixedPoint<> pitch{1.0};
        psyqo::FixedPoint<> sqr{1.0594630943592};
        if (pitchBase >= 0) {
            for (int i = 0; i < pitchBase; ++i) {
                pitch *= sqr;
            }
        } else {
            for (int i = 0; i < -pitchBase; ++i) {
                pitch /= sqr;
            }
        }
        if (pitchBase != 0) {
            soundPlayer.playSound(2, game.stepSound, pitch.value);
        } else {
            soundPlayer.playSound(2, game.drumSound, pitch.value);
        }
    }

    dialogueBox.handleInput(game.pad);

    songCounter += 42;
}

void GameplayScene::updateMusic()
{
    auto& soundPlayer = game.soundPlayer;
    const auto& events = game.midi.events;
    const auto numTracks = events.size();

    for (int j = 0; j < numTracks; ++j) {
        if (j == 0) {
            continue; // broken, lol
        }

        const auto& track = events[j];
        if (track.empty()) {
            continue;
        }

        // TODO: get from the instrument
        const auto& sample = [this](int track) -> const Sound& {
            switch (track) {
            case 1:
                return game.guitarSound;
            case 3:
                return game.melody2Sound;
            case 4:
                return game.stepSound;
            case 5:
                return game.guitarSound;
            case 6:
                return game.drumSound;
            }
            return game.emptySample;
        }(j);

        auto lastEventJ = lastEvent[j];
        const auto trSize = track.size();
        const auto startEvent = eventNum[j];
        for (int i = startEvent; i < trSize; ++i) {
            auto& event = track[i];
            if ((lastEventJ + event.delta) > musicTime) {
                lastEvent[j] = lastEventJ;
                eventNum[j] = i;
                break;
            }

            lastEventJ = lastEventJ + event.delta;
            if (event.type == MidiEvent::Type::NoteOn) {
                const auto note = event.param1;
                const auto voiceId = findChannel(j, note);
                if (voiceId == -1) {
                    // TODO: drop the oldest sample
                    continue;
                }

                // TODO: sample banks / VAB
                auto baseNote = [](int track) {
                    switch (track) {
                    case 0:
                        return 36;
                    case 1:
                        return 84;
                    case 3:
                        return 72;
                    case 5:
                        return 84;
                    default:
                        return 48;
                    }
                }(j);

                const auto pitchBase = note - baseNote;

                // TODO: LUT
                psyqo::FixedPoint<> pitch{1.0};
                psyqo::FixedPoint<> sqr{1.0594630943592};
                if (pitchBase >= 0) {
                    for (int i = 0; i < pitchBase; ++i) {
                        pitch *= sqr;
                    }
                } else {
                    for (int i = 0; i < -pitchBase; ++i) {
                        pitch /= sqr;
                    }
                }

                soundPlayer.playSound(voiceId, sample, pitch.value);
            } else if (event.type == MidiEvent::Type::NoteOff) {
                const auto note = event.param1;
                freeChannel(j, note);
            }
        }
    }
}

int GameplayScene::findChannel(int trackId, int noteId)
{
    for (int i = 0; i < usedChannels.size(); ++i) {
        if (usedChannels[i] == false) {
            channelUsers[i] = UseInfo{
                .trackId = trackId,
                .noteId = noteId,
            };
            usedChannels[i] = true;
            return i;
        }
    }
    return -1;
}

void GameplayScene::freeChannel(int trackId, int nodeId)
{
    for (int i = 0; i < usedChannels.size(); ++i) {
        if (usedChannels[i]) {
            auto& user = channelUsers[i];
            if (user.trackId == trackId && user.noteId == nodeId) {
                usedChannels[i] = false;
                game.soundPlayer.resetVoice(i);
            }
        }
    }
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
}

void GameplayScene::update()
{
    updateCamera();

    // spin the cat
    cato.rotation.y += 0.01;
    // cato.rotation.x = 0.25;

    dialogueBox.update();
}

void GameplayScene::draw()
{
    auto& ot = renderer.getOrderingTable();
    auto& primBuffer = renderer.getPrimBuffer();
    primBuffer.reset();

    // set dithering ON globally
    auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
    tpage.primitive.attr.setDithering(true);
    gpu().chain(tpage);

    // clear
    // psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
    psyqo::Color bg{{.r = 33, .g = 14, .b = 58}};
    auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
    gpu().getNextClear(fill.primitive, bg);
    gpu().chain(fill);

    const auto& gp = gpu();

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
                object.position.x.value = x * 4096;
                object.position.y.value = 0;
                object.position.z.value = z * 4096;
                renderer.drawMeshObject(object, camera, game.catoTexture);
            }
        }

        auto* tree = &game.levelModel.meshes[4];
        object.mesh = tree;
        for (int i = 0; i < 10; ++i) {
            object.position.x.value = i * 4096;
            object.position.z.value = 1 * 4096;
            renderer.drawMeshObject(object, camera, game.catoTexture);
        }

        auto* lamp = &game.levelModel.meshes[2];
        object.mesh = lamp;
        for (int i = 0; i < 10; ++i) {
            object.position.x.value = i * 4096;
            object.position.z.value = -1 * 4096;
            renderer.drawMeshObject(object, camera, game.catoTexture);
        }

        auto* car = &game.levelModel.meshes[5];
        renderer.bias = -100;
        object.mesh = car;
        object.position = {};
        object.position.x = 1.f;
        renderer.drawMeshObject(object, camera, game.catoTexture);
    }

    renderer.bias = 0;
    // draw dynamic objects
    {
        // TODO: first draw objects without rotation
        // (won't have to upload camera.viewRot and change PseudoRegister::Rotation then)

        {
            renderer.drawModelObject(cato, camera, game.catoTexture);
        }

        {
            // renderer.drawModelObject(car, camera, game.carTexture);
        }
    }

    gpu().chain(ot);

    // dialogueBox.draw(renderer, game.font, game.fontTexture, game.catoTexture);

    drawDebugInfo();
}

void GameplayScene::drawDebugInfo()
{
    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        textCol,
        "cam pos = (%.2f, %.2f, %.2f)",
        cato.position.x,
        cato.position.y,
        cato.position.z);

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 32}},
        textCol,
        "cam rot=(%.2f, %.2f)",
        camera.rotation.x,
        camera.rotation.y);

    const auto fps = gpu().getRefreshRate() / frameDiff;
    fpsMovingAverageNew = alpha * fps + oneMinAlpha * fpsMovingAverageOld;
    fpsMovingAverageOld = fpsMovingAverageNew;

    newFPS.value = fps << 12;
    // lerp
    avgFPS = avgFPS + lerpFactor * (newFPS - avgFPS);

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 48}},
        textCol,
        "FPS: %.2f, avg: %.2f",
        fpsMovingAverageNew,
        avgFPS);
}
