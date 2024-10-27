#include <psyqo-paths/cdrom-loader.hh>
#include <psyqo/application.hh>
#include <psyqo/bump-allocator.h>
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

#include "gte-math.h"
#include "matrix_test.h"

#include <common/syscalls/syscalls.h>

#include <cstdint>

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

struct TextureInfo {
    psyqo::PrimPieces::TPageAttr tpage;
    psyqo::PrimPieces::ClutIndex clut;
};

struct Camera {
    psyqo::Vec3 position{};
    psyqo::Vector<2, 10> rotation{};

    psyqo::Vec3 translation{};
    psyqo::Matrix33 viewRot;
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

    bool resourcesLoaded{false};
};

class GameplayScene : public psyqo::Scene {
    void start(StartReason reason) override;

    void onResourcesLoaded();

    void frame() override;

    void processInput();
    void update();
    void updateCamera();

    void draw();
    void drawDebugInfo();
    void drawLoadingScreen();

    void drawModelObject(
        const ModelObject& object,
        const Camera& camera,
        const TextureInfo& texture);
    void drawModel(const Model& model, const TextureInfo& texture);
    void drawMesh(const Mesh& mesh, const TextureInfo& texture);

    template<typename PrimType>
    int drawTris(const Mesh& mesh, const TextureInfo& texture, int numFaces, int vertexIdx);

    template<typename PrimType>
    int drawQuads(const Mesh& mesh, const TextureInfo& texture, int numFaces, int vertexIdx);

    static constexpr auto OT_SIZE = 4096 * 2;
    eastl::array<psyqo::OrderingTable<OT_SIZE>, 2> ots;

    static constexpr int PRIMBUFFLEN = 32768 * 8;
    eastl::array<psyqo::BumpAllocator<PRIMBUFFLEN>, 2> primBuffers;

    bool calledOnResourcesLoaded{false};

    // game objects
    ModelObject cato;
};

Game game;
GameplayScene gameplayScene;

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

    game->resourcesLoaded = true;
}

void Game::createScene()
{
    testing::testMatrix();
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    pad.initialize();

    pushScene(&gameplayScene);

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
    if (!game.resourcesLoaded) {
        drawLoadingScreen();
        return;
    }

    if (!calledOnResourcesLoaded) {
        onResourcesLoaded();
        calledOnResourcesLoaded = true;
    }

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
    psyqo::SoftMath::multiplyMatrix33(viewRotX, camera.viewRot, &camera.viewRot);

    // calculate camera translation vector
    camera.translation.x = -camera.position.x >> 12;
    camera.translation.y = -camera.position.y >> 12;
    camera.translation.z = -camera.position.z >> 12;

    psyqo::SoftMath::matrixVecMul3(camera.viewRot, camera.translation, &camera.translation);
}

void GameplayScene::update()
{
    updateCamera();

    // spin the cat
    cato.rotation.y += 0.01;
    cato.rotation.x = 0.25;
}

void GameplayScene::drawLoadingScreen()
{
    const auto parity = gpu().getParity();
    auto& ot = ots[parity];

    auto& primBuffer = primBuffers[parity];
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

void GameplayScene::draw()
{
    const auto parity = gpu().getParity();

    auto& ot = ots[parity];
    auto& primBuffer = primBuffers[parity];
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

    // draw static objects
    {
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(camera.translation);
        drawModel(game.levelModel, game.bricksTexture);
    }

    // draw dynamic objects
    {
        drawModelObject(gameplayScene.cato, camera, game.catoTexture);
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

void GameplayScene::drawModelObject(
    const ModelObject& object,
    const Camera& camera,
    const TextureInfo& texture)
{
    if (object.rotation.x == 0.0 && object.rotation.y == 0.0) {
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);
    } else {
        // yaw
        auto objectRotMat = psyqo::SoftMath::
            generateRotationMatrix33(object.rotation.y, psyqo::SoftMath::Axis::Y, game.trig);

        if (cato.rotation.x != 0.0) { // pitch
            const auto rotX = psyqo::SoftMath::
                generateRotationMatrix33(object.rotation.x, psyqo::SoftMath::Axis::X, game.trig);
            psyqo::SoftMath::multiplyMatrix33(objectRotMat, rotX, &objectRotMat);
        }

        psyqo::SoftMath::multiplyMatrix33(camera.viewRot, objectRotMat, &objectRotMat);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(objectRotMat);
    }

    auto posCamSpace = object.position;
    psyqo::SoftMath::matrixVecMul3(camera.viewRot, posCamSpace, &posCamSpace);
    posCamSpace += camera.translation;

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

    drawModel(*object.model, texture);
}

void GameplayScene::drawModel(const Model& model, const TextureInfo& texture)
{
    for (const auto& mesh : model.meshes) {
        drawMesh(mesh, texture);
    }
}

void GameplayScene::drawMesh(const Mesh& mesh, const TextureInfo& texture)
{
    std::size_t vertexIdx = 0;
    vertexIdx =
        drawTris<psyqo::Prim::GouraudTriangle>(mesh, texture, mesh.numUntexturedTris, vertexIdx);
    vertexIdx =
        drawQuads<psyqo::Prim::GouraudQuad>(mesh, texture, mesh.numUntexturedQuads, vertexIdx);
    vertexIdx =
        drawTris<psyqo::Prim::GouraudTexturedTriangle>(mesh, texture, mesh.numTris, vertexIdx);
    vertexIdx =
        drawQuads<psyqo::Prim::GouraudTexturedQuad>(mesh, texture, mesh.numQuads, vertexIdx);
}

template<typename PrimType>
int GameplayScene::drawTris(
    const Mesh& mesh,
    const TextureInfo& texture,
    int numFaces,
    int vertexIdx)
{
    const auto parity = gpu().getParity();
    auto& ot = ots[parity];
    auto& primBuffer = primBuffers[parity];

    for (int i = 0; i < numFaces; ++i, vertexIdx += 3) {
        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
        psyqo::GTE::Kernels::rtpt();

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            continue;
        }

        psyqo::GTE::Kernels::avsz3();
        const auto avgZ =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ < 0 || avgZ >= OT_SIZE) {
            continue;
        }

        auto& triFrag = primBuffer.allocateFragment<PrimType>();
        auto& tri2d = triFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

        tri2d.interpolateColors(&v0.col, &v1.col, &v2.col);

        if constexpr (eastl::is_same_v<PrimType, psyqo::Prim::GouraudTexturedTriangle>) {
            tri2d.uvA.u = v0.uv.vx;
            tri2d.uvA.v = v0.uv.vy;
            tri2d.uvB.u = v1.uv.vx;
            tri2d.uvB.v = v1.uv.vy;
            tri2d.uvC.u = v2.uv.vx;
            tri2d.uvC.v = v2.uv.vy;

            tri2d.tpage = texture.tpage;
            tri2d.clutIndex = texture.clut;
        }

        ot.insert(triFrag, avgZ);
    }

    return vertexIdx;
}

template<typename PrimType>
int GameplayScene::drawQuads(
    const Mesh& mesh,
    const TextureInfo& texture,
    int numFaces,
    int vertexIdx)
{
    const auto parity = gpu().getParity();
    auto& ot = ots[parity];
    auto& primBuffer = primBuffers[parity];

    for (int i = 0; i < numFaces; ++i, vertexIdx += 4) {
        const auto& v0 = mesh.vertices[vertexIdx + 0];
        const auto& v1 = mesh.vertices[vertexIdx + 1];
        const auto& v2 = mesh.vertices[vertexIdx + 2];
        const auto& v3 = mesh.vertices[vertexIdx + 3];

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
        psyqo::GTE::Kernels::rtpt();

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            continue;
        }

        auto& quadFrag = primBuffer.allocateFragment<PrimType>();
        auto& quad2d = quadFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::Kernels::avsz4();
        const auto avgZ =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ < 0 || avgZ >= OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

        quad2d.interpolateColors(&v0.col, &v1.col, &v2.col, &v3.col);

        if constexpr (eastl::is_same_v<PrimType, psyqo::Prim::GouraudTexturedQuad>) {
            quad2d.uvA.u = v0.uv.vx;
            quad2d.uvA.v = v0.uv.vy;
            quad2d.uvB.u = v1.uv.vx;
            quad2d.uvB.v = v1.uv.vy;
            quad2d.uvC.u = v2.uv.vx;
            quad2d.uvC.v = v2.uv.vy;
            quad2d.uvD.u = v3.uv.vx;
            quad2d.uvD.v = v3.uv.vy;

            quad2d.tpage = texture.tpage;
            quad2d.clutIndex = texture.clut;
        }

        ot.insert(quadFrag, avgZ);
    }

    return vertexIdx;
}

int main()
{
    return game.run();
}
