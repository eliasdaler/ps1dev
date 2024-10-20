#include <psyqo/application.hh>
#include <psyqo/font.hh>
#include <psyqo/gpu.hh>
#include <psyqo/scene.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/soft-math.hh>
#include <psyqo/coroutine.hh>
#include <psyqo/simplepad.hh>

#include <psyqo/cdrom-device.hh>
#include <psyqo/iso9660-parser.hh>
#include <psyqo-paths/cdrom-loader.hh>

#include <psyqo/bump-allocator.h>

#include <common/syscalls/syscalls.h>

#include <cstdint>

#include "TimFile.h"
#include "Model.h"
#include "Object.h"

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

psyqo::Color interpColor(const psyqo::Color& c)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(&c.packed);

    psyqo::GTE::Kernels::dpcs();

    psyqo::Color col;
    col.packed = (uint32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2, psyqo::GTE::Safe>();
    return col;
};

eastl::array<psyqo::Color, 3> interpColor3(
    const psyqo::Color& c0,
    const psyqo::Color& c1,
    const psyqo::Color& c2)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB0, psyqo::GTE::Unsafe>(&c0.packed);
    psyqo::GTE::write<psyqo::GTE::Register::RGB1, psyqo::GTE::Unsafe>(&c1.packed);
    psyqo::GTE::write<psyqo::GTE::Register::RGB2, psyqo::GTE::Safe>(&c2.packed);

    psyqo::GTE::Kernels::dpct();

    eastl::array<psyqo::Color, 3> cols;
    cols[0].packed = (uint32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::RGB0, psyqo::GTE::Safe>();
    cols[1].packed =
        (uint32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::RGB1, psyqo::GTE::Unsafe>();
    cols[2].packed =
        (uint32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2, psyqo::GTE::Unsafe>();
    return cols;
}

struct TextureInfo {
    psyqo::PrimPieces::TPageAttr tpage;
    psyqo::PrimPieces::ClutIndex clut;
};

class Game : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    [[nodiscard]] TextureInfo uploadTIM();

    psyqo::Font<> romFont;
    psyqo::Trig<> trig;

    psyqo::CDRomDevice cdrom;
    psyqo::ISO9660Parser isoParser = psyqo::ISO9660Parser(&cdrom);
    psyqo::paths::CDRomLoader cdromLoader;
    eastl::vector<std::uint8_t> cdReadBuffer;
    psyqo::Coroutine<> cdLoadCoroutine;

    psyqo::SimplePad pad;

    bool texturesLoaded{false};
    Model levelModel;
    Model catoModel;

    TextureInfo bricksTexture;
    TextureInfo catoTexture;
};

using Short = psyqo::GTE::Short;

class GameplayScene final : public psyqo::Scene {
    void start(StartReason reason) override
    {
        // fov
        psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(h);

        // screen "center" (screenWidth / 2, screenHeight / 2)
        psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(
            psyqo::FixedPoint<16>(SCREEN_WIDTH / 2.0).raw());
        psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(
            psyqo::FixedPoint<16>(SCREEN_HEIGHT / 2.0).raw());

        // FIXME: use OT_SIZE here somehow?
        psyqo::GTE::write<psyqo::GTE::Register::ZSF3, psyqo::GTE::Unsafe>(1024 / 3);
        psyqo::GTE::write<psyqo::GTE::Register::ZSF4, psyqo::GTE::Unsafe>(1024 / 4);

        SetFogNearFar(5000, 20800, SCREEN_WIDTH / 2);
        // far color
        const auto farColor = psyqo::Color{.r = 0, .g = 0, .b = 0};
        psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(farColor.r);
        psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(farColor.g);
        psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Unsafe>(farColor.b);

        cato.position.y = 0.88;
    }

    void frame() override;

    void drawModel(const Model& model, const TextureInfo& texture);
    void drawMesh(const Mesh& mesh, const TextureInfo& texture);

    template<typename PrimType>
    int drawTris(const Mesh& mesh, const TextureInfo& texture, int numFaces, int vertexIdx);

    template<typename PrimType>
    int drawQuads(const Mesh& mesh, const TextureInfo& texture, int numFaces, int vertexIdx);

    static constexpr auto OT_SIZE = 4096 * 2;
    eastl::array<psyqo::OrderingTable<OT_SIZE>, 2> ots;

    psyqo::Vec3 camPos{3588.f, -1507.f, -10259.f};
    psyqo::Vec3 camRot{-150.f, -300.f, 0.f};

    ModelObject cato;

    static constexpr int PRIMBUFFLEN = 32768 * 8;
    eastl::array<psyqo::BumpAllocator<PRIMBUFFLEN>, 2> primBuffers;

    std::uint16_t h = 300; // "fov"
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

    game->cdromLoader.readFile(
        "LEVEL.BIN;1", game->gpu(), game->isoParser, [game](eastl::vector<uint8_t>&& buffer) {
            game->cdReadBuffer = eastl::move(buffer);
            game->levelModel.load(game->cdReadBuffer);
            game->cdLoadCoroutine.resume();
        });
    co_await awaiter;

    game->cdromLoader.readFile(
        "CATO.BIN;1", game->gpu(), game->isoParser, [game](eastl::vector<uint8_t>&& buffer) {
            game->cdReadBuffer = eastl::move(buffer);
            game->catoModel.load(game->cdReadBuffer);
            game->cdLoadCoroutine.resume();
        });
    co_await awaiter;

    game->cdromLoader.readFile(
        "BRICKS.TIM;1", game->gpu(), game->isoParser, [game](eastl::vector<uint8_t>&& buffer) {
            game->cdReadBuffer = eastl::move(buffer);
            game->bricksTexture = game->uploadTIM();
            game->cdLoadCoroutine.resume();
        });
    co_await awaiter;

    game->cdromLoader.readFile(
        "CATO.TIM;1", game->gpu(), game->isoParser, [game](eastl::vector<uint8_t>&& buffer) {
            game->cdReadBuffer = eastl::move(buffer);
            game->catoTexture = game->uploadTIM();
            game->cdLoadCoroutine.resume();
        });
    co_await awaiter;

    game->texturesLoaded = true;
}

void Game::createScene()
{
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    // TODO: check if it's possible to call it in game init?
    pad.initialize();

    pushScene(&gameplayScene);

    cdLoadCoroutine = loadCoroutine(this);
    cdLoadCoroutine.resume();
}

TextureInfo Game::uploadTIM()
{
    TimFile tim = readTimFile(cdReadBuffer);
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

void GameplayScene::frame()
{
    if (!game.texturesLoaded) {
        return;
    }

    psyqo::Angle angleX;
    psyqo::Angle angleY;
    angleX.value = (camRot.x.value / 2) >> 12;
    angleY.value = (camRot.y.value / 2) >> 12;

    { // input
        const auto& pad = game.pad;
        const auto walkSpeed = 64;
        const auto rotateSpeed = 16;

        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Left)) {
            camRot.y -= rotateSpeed;
        }
        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Right)) {
            camRot.y += rotateSpeed;
        }

        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Up)) {
            camPos.x += game.trig.sin(angleY) * walkSpeed;
            camPos.z += game.trig.cos(angleY) * walkSpeed;
        }
        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Down)) {
            camPos.x -= game.trig.sin(angleY) * walkSpeed;
            camPos.z -= game.trig.cos(angleY) * walkSpeed;
        }

        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::L2)) {
            camRot.x += rotateSpeed;
        }
        if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::R2)) {
            camRot.x -= rotateSpeed;
        }
    }

    // calculate camera rotation matrix
    // input might have changed the values
    angleX.value = (camRot.x.value / 2) >> 12;
    angleY.value = (camRot.y.value / 2) >> 12;
    auto rotX =
        psyqo::SoftMath::generateRotationMatrix33(angleX, psyqo::SoftMath::Axis::X, game.trig);
    const auto rotY =
        psyqo::SoftMath::generateRotationMatrix33(angleY, psyqo::SoftMath::Axis::Y, game.trig);
    psyqo::SoftMath::multiplyMatrix33(rotY, rotX, &rotX);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(rotX);

    // calculate and upload camera translation
    psyqo::Vec3 camTrans{};
    camTrans.x = -camPos.x >> 12;
    camTrans.y = -camPos.y >> 12;
    camTrans.z = -camPos.z >> 12;

    psyqo::SoftMath::matrixVecMul3(rotX, camTrans, &camTrans);

    psyqo::GTE::write<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>(camTrans.x.raw());
    psyqo::GTE::write<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>(camTrans.y.raw());
    psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>(camTrans.z.raw());

    const auto parity = gpu().getParity();

    auto& ot = ots[parity];
    auto& primBuffer = primBuffers[parity];
    primBuffer.reset();

    // draw

    // set dithering ON globally
    auto& tpage = primBuffer.allocate<psyqo::Prim::TPage>();
    tpage.primitive.attr.setDithering(true);
    gpu().chain(tpage);

    psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
    auto& fill = primBuffer.allocate<psyqo::Prim::FastFill>();
    gpu().getNextClear(fill.primitive, bg);
    gpu().chain(fill);

    drawModel(game.levelModel, game.bricksTexture);

    {
        auto& cato = gameplayScene.cato;

        /* psyqo::Angle angleX;
        psyqo::Angle angleY;
        angleX.value = (cato.rotation.x.value / 2) >> 12;
        angleY.value = (cato.rotation.y.value / 2) >> 12;
        auto worldRotX = psyqo::SoftMath::
            generateRotationMatrix33(angleX, psyqo::SoftMath::Axis::X, game.m_trig);
        const auto worldRotY = psyqo::SoftMath::
            generateRotationMatrix33(angleY, psyqo::SoftMath::Axis::Y, game.m_trig);

        psyqo::SoftMath::multiplyMatrix33(worldRotY, worldRotX, &worldRotX);
        psyqo::SoftMath::multiplyMatrix33(rotX, worldRotX, &worldRotX);

        auto posCamSpace = cato.position - camPos;
        psyqo::SoftMath::matrixVecMul3(rotX, posCamSpace, &posCamSpace);

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(worldRotX);
        psyqo::GTE::write<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>(posCamSpace.x.raw());
        psyqo::GTE::write<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>(posCamSpace.y.raw());
        psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>(posCamSpace.z.raw()); */

        auto posCamSpace = cato.position + camTrans;
        psyqo::GTE::write<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>(posCamSpace.x.raw());
        psyqo::GTE::write<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>(posCamSpace.y.raw());
        psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>(posCamSpace.z.raw());

        drawModel(game.catoModel, game.catoTexture);
    }

    gpu().chain(ot);

    // debug
    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
    game.romFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        textCol,
        "TRX: %d, TRY: %d, TRZ: %d",
        camTrans.x.raw(),
        camTrans.y.raw(),
        camTrans.z.raw());

    game.romFont
        .chainprintf(game.gpu(), {{.x = 16, .y = 32}}, textCol, "RX: %d, RY: %d", angleX, angleY);
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

        auto& triFrag = primBuffer.allocate<PrimType>();
        auto& tri2d = triFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

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

        const auto cols = interpColor3(v0.col, v1.col, v2.col);
        tri2d.setColorA(cols[0]);
        tri2d.setColorB(cols[1]);
        tri2d.setColorC(cols[2]);

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

        auto& quadFrag = primBuffer.allocate<PrimType>();
        auto& quad2d = quadFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointC.packed);

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            continue;
        }

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);
        psyqo::GTE::Kernels::avsz4();
        const auto avgZ =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ < 0 || avgZ >= OT_SIZE) {
            continue;
        }

        const auto cols = interpColor3(v0.col, v1.col, v2.col);
        quad2d.setColorA(cols[0]);
        quad2d.setColorB(cols[1]);
        quad2d.setColorC(cols[2]);
        quad2d.setColorD(interpColor(v3.col));

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
