#include <psyqo-paths/cdrom-loader.hh>
#include <psyqo/application.hh>
#include <psyqo/cdrom-device.hh>
#include <psyqo/coroutine.hh>
#include <psyqo/font.hh>
#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/iso9660-parser.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/scene.hh>
#include <psyqo/simplepad.hh>
#include <psyqo/soft-math.hh>

#include "Renderer.h"
#include "TextureInfo.h"
#include "gte-math.h"
#include "matrix_test.h"

#include <common/syscalls/syscalls.h>

#include <cstdint>

#include "Camera.h"
#include "Model.h"
#include "Object.h"
#include "TimFile.h"

#include <psyqo/xprintf.h>

#include <psyqo/kernel.hh>

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

namespace
{

static constexpr auto SCREEN_WIDTH = 320;
static constexpr auto SCREEN_HEIGHT = 240;

void SetFogNearFar(int a, int b, int h)
{
    // TODO: check params + add asserts?
    const auto dqa = ((-a * b / (b - a)) << 8) / h;
    const auto dqaF = eastl::clamp(dqa, -32767, 32767);
    const auto dqbF = ((b << 12) / (b - a) << 12);

    psyqo::GTE::write<psyqo::GTE::Register::DQA, psyqo::GTE::Unsafe>(dqaF);
    psyqo::GTE::write<psyqo::GTE::Register::DQB, psyqo::GTE::Safe>(dqbF);
};

class Game : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    void loadTIM(eastl::string_view filename, TextureInfo& textureInfo);
    void loadModel(eastl::string_view filename, Model& model);

    [[nodiscard]] TextureInfo uploadTIM(const TimFile& tim);

    psyqo::Font<> romFont;
    psyqo::Trig<> trig;

    psyqo::CDRomDevice cdrom;
    psyqo::ISO9660Parser isoParser{&cdrom};
    psyqo::paths::CDRomLoader cdromLoader;
    eastl::vector<std::uint8_t> cdReadBuffer;
    psyqo::Coroutine<> cdLoadCoroutine;

    psyqo::SimplePad pad;

    Model levelModel;
    Model catoModel;

    TextureInfo bricksTexture;
    TextureInfo catoTexture;

    Camera camera;
};

class GameplayScene : public psyqo::Scene {
public:
    void onResourcesLoaded();

private:
    void start(StartReason reason) override;

    void frame() override;

    void processInput();
    void update();
    void updateCamera();

    void draw();
    void drawDebugInfo();

    // game objects
    ModelObject cato;
};

class LoadingScene : public psyqo::Scene {
    void start(StartReason reason) override;

    void frame() override;

    void draw();
};

Game game;
Renderer renderer;
GameplayScene gameplayScene;
LoadingScene loadingScene;

} // namespace

void Game::prepare()
{
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::NTSC)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
    cdrom.prepare();
}

psyqo::Coroutine<> loadCoroutine(Game* game)
{
    psyqo::Coroutine<>::Awaiter awaiter = game->cdLoadCoroutine.awaiter();

    game->loadModel("LEVEL.BIN;1", game->levelModel);
    co_await awaiter;

    game->loadModel("CATO.BIN;1", game->catoModel);
    co_await awaiter;

    game->loadTIM("BRICKS.TIM;1", game->bricksTexture);
    co_await awaiter;

    game->loadTIM("CATO.TIM;1", game->catoTexture);
    co_await awaiter;

    game->popScene();

    gameplayScene.onResourcesLoaded();
    game->pushScene(&gameplayScene);
}

void Game::createScene()
{
    testing::testMatrix();
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    pad.initialize();

    // pushScene(&gameplayScene);
    pushScene(&loadingScene);

    // load resources
    cdLoadCoroutine = loadCoroutine(this);
    cdLoadCoroutine.resume();
}

void Game::loadTIM(eastl::string_view filename, TextureInfo& textureInfo)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, &textureInfo](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            const auto tim = readTimFile(cdReadBuffer);
            textureInfo = uploadTIM(tim);
            cdLoadCoroutine.resume();
        });
}

void Game::loadModel(eastl::string_view filename, Model& model)
{
    cdromLoader
        .readFile(filename, gpu(), isoParser, [this, &model](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            model.load(cdReadBuffer);
            cdLoadCoroutine.resume();
        });
}

TextureInfo Game::uploadTIM(const TimFile& tim)
{
    psyqo::Rect region =
        {.pos = {{.x = (std::int16_t)tim.pixDX, .y = (std::int16_t)tim.pixDY}},
         .size = {{.w = (std::int16_t)tim.pixW, .h = (std::int16_t)tim.pixH}}};
    gpu().uploadToVRAM((uint16_t*)tim.pixelsIdx.data(), region);

    // upload CLUT(s)
    // FIXME: support multiple cluts
    region =
        {.pos = {{.x = (std::int16_t)tim.clutDX, .y = (std::int16_t)tim.clutDY}},
         .size = {{.w = (std::int16_t)tim.clutW, .h = (std::int16_t)tim.clutH}}};
    gpu().uploadToVRAM(tim.cluts[0].colors.data(), region);

    TextureInfo info;
    info.clut = {{.x = tim.clutDX, .y = tim.clutDY}};

    const auto colorMode = [](TimFile::PMode pmode) {
        switch (pmode) {
        case TimFile::PMode::Clut4Bit:
            return psyqo::Prim::TPageAttr::Tex4Bits;
            break;
        case TimFile::PMode::Clut8Bit:
            return psyqo::Prim::TPageAttr::Tex8Bits;
            break;
        case TimFile::PMode::Direct15Bit:
            return psyqo::Prim::TPageAttr::Tex16Bits;
            break;
        }
        psyqo::Kernel::assert(false, "unexpected pmode value");
        return psyqo::Prim::TPageAttr::Tex16Bits;
    }(tim.pmode);

    info.tpage.setPageX((std::uint8_t)(tim.pixDX / 64))
        .setPageY((std::uint8_t)(tim.pixDY / 128))
        .setDithering(true)
        .set(colorMode);

    return info;
}

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

    // game.camera.position = cato.position;
    game.camera.position = {3588.f, -1507.f, -10259.f};
    game.camera.rotation = {-0.1f, -0.1f};
}

void GameplayScene::frame()
{
    processInput();
    update();
    draw();
}

void GameplayScene::processInput()
{
    auto& camera = game.camera;

    const auto& pad = game.pad;
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
        camera.position.x += game.trig.sin(camera.rotation.y) * walkSpeed;
        camera.position.z += game.trig.cos(camera.rotation.y) * walkSpeed;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Down)) {
        camera.position.x -= game.trig.sin(camera.rotation.y) * walkSpeed;
        camera.position.z -= game.trig.cos(camera.rotation.y) * walkSpeed;
    }
}

void GameplayScene::updateCamera()
{
    // calculate camera rotation matrix
    auto& camera = game.camera;

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

    const auto& camera = game.camera;

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
        renderer.drawModelObject(gp, gameplayScene.cato, camera, game.catoTexture);
    }

    gpu().chain(ot);

    drawDebugInfo();
}

void GameplayScene::drawDebugInfo()
{
    const auto& camera = game.camera;

    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        textCol,
        "cam pos=(%d, %d, %d)",
        camera.position.x.raw() >> 12,
        camera.position.y.raw() >> 12,
        camera.position.z.raw() >> 12);

    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 32}},
        textCol,
        "cam rot=(%d, %d)",
        camera.rotation.x.raw(),
        camera.rotation.y.raw());
}

void LoadingScene::start(StartReason reason)
{}

void LoadingScene::frame()
{
    draw();
}

void LoadingScene::draw()
{
    const auto parity = gpu().getParity();
    auto& ot = renderer.ots[parity];

    auto& primBuffer = renderer.primBuffers[parity];
    primBuffer.reset();
    // fill bg
    psyqo::Color bg{{.r = 0, .g = 0, .b = 0}};
    auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
    gpu().getNextClear(fill.primitive, bg);
    gpu().chain(fill);
    // text
    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
    game.romFont.chainprintf(game.gpu(), {{.x = 16, .y = 32}}, textCol, "Loading...");
}

int main()
{
    return game.run();
}
