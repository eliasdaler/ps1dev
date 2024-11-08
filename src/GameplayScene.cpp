#include "GameplayScene.h"

#include <psyqo/gte-registers.hh>
#include <psyqo/simplepad.hh>
#include <psyqo/soft-math.hh>

#include <psyqo/primitives/rectangles.hh>
#include <psyqo/primitives/sprites.hh>

#include "Common.h"
#include "Game.h"
#include "Renderer.h"
#include "gte-math.h"

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

    SetFogNearFar(5000, 20800, SCREEN_WIDTH / 2);
    // far color
    const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
    psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(farColor.r);
    psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(farColor.g);
    psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Unsafe>(farColor.b);
}

void GameplayScene::onResourcesLoaded()
{
    cato.model = &game.catoModel;
    cato.position = {0.0, 0.88, 0.8};

    camera.position = {3588.f, -1507.f, -10259.f};
    camera.rotation = {-0.1f, -0.1f};
}

void GameplayScene::frame()
{
    processInput();
    update();
    draw();
}

void GameplayScene::processInput()
{
    const auto& pad = game.pad;
    const auto& trig = game.trig;

    constexpr auto walkSpeed = 64;
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
}

void GameplayScene::updateCamera()
{
    // calculate camera rotation matrix
    camera.viewRot = psyqo::SoftMath::
        generateRotationMatrix33(camera.rotation.y, psyqo::SoftMath::Axis::Y, game.trig);
    const auto viewRotX = psyqo::SoftMath::
        generateRotationMatrix33(camera.rotation.x, psyqo::SoftMath::Axis::X, game.trig);

    // psyqo::SoftMath::multiplyMatrix33(viewRotX, camera.viewRot, &camera.viewRot);
    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(viewRotX, camera.viewRot, &camera.viewRot);

    // calculate camera translation vector
    camera.translation.x = -camera.position.x >> 12;
    camera.translation.y = -camera.position.y >> 12;
    camera.translation.z = -camera.position.z >> 12;

    // psyqo::SoftMath::matrixVecMul3(camera.viewRot, camera.translation, &camera.translation);
    psyqo::GTE::Math::matrixVecMul3<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(camera.viewRot, camera.translation, &camera.translation);
}

void GameplayScene::update()
{
    updateCamera();

    // spin the cat
    cato.rotation.y += 0.01;
    cato.rotation.x = 0.25;
}

void GameplayScene::draw()
{
    const auto parity = gpu().getParity();

    auto& ot = renderer.ots[parity];
    auto& primBuffer = renderer.primBuffers[parity];
    primBuffer.reset();

    // set dithering ON globally
    auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
    tpage.primitive.attr.setDithering(true);
    gpu().chain(tpage);

    // clear
    psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
    auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
    gpu().getNextClear(fill.primitive, bg);
    gpu().chain(fill);

    const auto& gp = gpu();
    // draw static objects
    {
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(camera.translation);
        renderer.drawModel(gp, game.levelModel, game.bricksTexture);
    }

    // draw dynamic objects
    {
        // TODO: first draw objects without rotation
        // (won't have to upload camera.viewRot and change PseudoRegister::Rotation then)
        renderer.drawModelObject(gp, cato, camera, game.catoTexture);
    }

    gpu().chain(ot);

    static constexpr auto dbX = 60;
    static constexpr auto dbY = 140;
    static constexpr auto dbW = 200;
    static constexpr auto dbH = 64;

    { // bg rect
        auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
        auto& rect = rectFrag.primitive;
        rect.position.x = dbX;
        rect.position.y = dbY;
        rect.size.x = dbW;
        rect.size.y = dbH;

        static const auto black = psyqo::Color{{0, 0, 0}};
        rect.setColor(black);
        rect.setSemiTrans();
        gpu().chain(rectFrag);
    }

    static const eastl::fixed_string<char, 256> textString{"AAAAAAAAAAAA\nBB \2AA \1BB"};

    struct GlyphInfo {
        int u;
        int v;
    };

    static GlyphInfo glyphInfos[255];
    glyphInfos['A'] = GlyphInfo{0, 144};
    glyphInfos['B'] = GlyphInfo{16, 144};

    static const auto white = psyqo::Color{{128, 128, 128}};
    static const auto red = psyqo::Color{{255, 0, 0}};

    static constexpr auto textOffsetX = 0;
    static constexpr auto textOffsetY = 0;
    static const auto glyphW = 16;
    static const auto glyphH = 16;
    static const auto lineH = 16;

    auto currColor = white;
    auto glyphX = dbX + textOffsetX;
    auto glyphY = dbY + textOffsetY;

    static int numOfCharsToShow = 0;
    static int timer = 0;

    { // sprites
        auto& fontAtlasTexture = game.catoTexture;

        // set tpage
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr = fontAtlasTexture.tpage;
        gpu().chain(tpage);

        if (timer == 2) {
            timer = 0;
            ++numOfCharsToShow;
        } else {
            ++timer;
        }

        if (game.pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Cross)) {
            numOfCharsToShow = 0;
        }

        int numCharsShown = 0;
        for (int i = 0; i < textString.size(); ++i) {
            auto c = textString[i];

            if (c == '\n') {
                glyphX = dbX + textOffsetX;
                glyphY += lineH;
                continue;
            }

            if (c == ' ') {
                glyphX += glyphW; // TODO: kern here
                continue;
            }

            if (c == 1) {
                currColor = white;
                continue;
            } else if (c == 2) {
                currColor = red;
                continue;
            }

            auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite16x16>();
            auto& sprite = spriteFrag.primitive;
            sprite.texInfo.clut = fontAtlasTexture.clut;

            sprite.position.x = glyphX;
            sprite.position.y = glyphY;
            auto& glyphInfo = glyphInfos[(int)c];
            sprite.texInfo.u = glyphInfo.u;
            sprite.texInfo.v = glyphInfo.v;

            sprite.setColor(currColor);
            gpu().chain(spriteFrag);

            glyphX += glyphW; // TODO: kern here
            ++numCharsShown;
            if (numCharsShown > numOfCharsToShow) {
                break;
            }
        }
    }

    drawDebugInfo();
}

void GameplayScene::drawDebugInfo()
{
    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        textCol,
        "cam pos=(%.2f, %.2f, %.2f)",
        camera.position.x,
        camera.position.y,
        camera.position.z);

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 32}},
        textCol,
        "cam rot=(%d, %d)",
        camera.rotation.x.raw(),
        camera.rotation.y.raw());
}
