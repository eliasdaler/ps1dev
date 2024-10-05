#include <psyqo/application.hh>
#include <psyqo/font.hh>
#include <psyqo/gpu.hh>
#include <psyqo/scene.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/soft-math.hh>
#include <psyqo/coroutine.hh>

#include <psyqo/cdrom-device.hh>
#include <psyqo/iso9660-parser.hh>
#include <psyqo-paths/cdrom-loader.hh>

#include <common/syscalls/syscalls.h>

#include <cstdint>

#include "TimFile.h"
#include "Model.h"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

namespace
{

class Game final : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    void uploadTIM();

    psyqo::Font<> m_systemFont;
    psyqo::Font<> m_romFont;
    psyqo::Trig<> m_trig;

    psyqo::CDRomDevice m_cdrom;
    psyqo::ISO9660Parser m_isoParser = psyqo::ISO9660Parser(&m_cdrom);
    psyqo::paths::CDRomLoader m_cdromLoader;
    eastl::vector<uint8_t> m_buffer;

    bool texturesLoaded{false};
    Model levelModel;

    psyqo::Coroutine<> m_coroutine;
};

struct Quad {
    psyqo::GTE::PackedVec3 verts[4];
};

using Short = psyqo::GTE::Short;

class GameplayScene final : public psyqo::Scene {
    void start(StartReason reason) override
    {
        // fov
        psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(h);

        // screen "center" (screenWidth / 2, screenHeight / 2)
        psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(
            psyqo::FixedPoint<16>(160.0).raw());
        psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(
            psyqo::FixedPoint<16>(120.0).raw());
    }

    void frame() override;

    psyqo::OrderingTable<4096> ots[2];

    psyqo::Vec3 camPos{-500.f, -180.f, -1000.f};
    psyqo::Vec3 camRot{0.f, 230.f, 0.f};

    static constexpr int PRIMBUFFLEN = 32768 * 8;
    std::byte primBytes[2][PRIMBUFFLEN];
    std::byte* nextpri{nullptr}; // pointer to primbuff

    uint16_t h = 300;
};

// We're instantiating the two objects above right now.
Game game;
GameplayScene gameplayScene;

} // namespace

void Game::prepare()
{
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
    m_cdrom.prepare();
}

psyqo::Coroutine<> loadCoroutine(Game* game)
{
    psyqo::Coroutine<>::Awaiter awaiter = game->m_coroutine.awaiter();

    ramsyscall_printf("Loading BRICKS.TIM...\n");
    game->m_cdromLoader.readFile(
        "BRICKS.TIM;1", game->gpu(), game->m_isoParser, [game](eastl::vector<uint8_t>&& buffer) {
            game->m_buffer = eastl::move(buffer);
            game->uploadTIM();
            game->m_coroutine.resume();
        });
    co_await awaiter;

    ramsyscall_printf("Loading CATO.TIM...\n");
    game->m_cdromLoader.readFile(
        "CATO.TIM;1", game->gpu(), game->m_isoParser, [game](eastl::vector<uint8_t>&& buffer) {
            game->m_buffer = eastl::move(buffer);
            game->uploadTIM();
            game->m_coroutine.resume();
        });
    co_await awaiter;

    ramsyscall_printf("Loading LEVEL.BIN...\n");
    game->m_cdromLoader.readFile(
        "LEVEL.BIN;1", game->gpu(), game->m_isoParser, [game](eastl::vector<uint8_t>&& buffer) {
            game->m_buffer = eastl::move(buffer);
            game->levelModel.load(game->m_buffer);
            game->m_coroutine.resume();
        });
    co_await awaiter;

    game->texturesLoaded = true;
}

void Game::createScene()
{
    m_systemFont.uploadSystemFont(gpu());
    m_romFont.uploadKromFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});
    pushScene(&gameplayScene);

    m_coroutine = loadCoroutine(this);
    m_coroutine.resume();
}

void Game::uploadTIM()
{
    TimFile tim = readTimFile(m_buffer);
    psyqo::Rect region =
        {.pos = {{.x = (std::int16_t)tim.pixDX, .y = (std::int16_t)tim.pixDY}},
         .size = {{.w = (std::int16_t)tim.pixW, .h = (std::int16_t)tim.pixH}}};
    gpu().uploadToVRAM((uint16_t*)tim.pixelsIdx.data(), region);

    // upload CLUT(s)
    // FIXME: support multiple cluts
    region =
        {.pos = {{.x = (std::int16_t)tim.clutDX, .y = (std::int16_t)tim.clutDY}},
         .size = {{.w = (std::int16_t)tim.clutW, .h = (std::int16_t)tim.clutH}}};
    gpu().uploadToVRAM((uint16_t*)tim.cluts[0].colors.data(), region);
}

void GameplayScene::frame()
{
    if (!game.texturesLoaded) {
        return;
    }

    // calculate camera rotation matrix
    psyqo::Angle m_angleX;
    psyqo::Angle m_angleY;
    m_angleX.value = (camRot.x.value / 2) >> 12;
    m_angleY.value = (camRot.y.value / 2) >> 12;
    auto rotX =
        psyqo::SoftMath::generateRotationMatrix33(m_angleX, psyqo::SoftMath::Axis::X, game.m_trig);
    const auto rotY =
        psyqo::SoftMath::generateRotationMatrix33(m_angleY, psyqo::SoftMath::Axis::Y, game.m_trig);
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

    auto& primBuf = primBytes[parity];
    nextpri = &primBuf[0];

    // draw
    psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
    using FastFillPrim = psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill>;
    auto& fill = *(FastFillPrim*)nextpri;
    gpu().getNextClear(fill.primitive, bg);
    gpu().chain(fill);
    nextpri += sizeof(FastFillPrim);

    int32_t zValues[4];

    for (const auto& mesh : game.levelModel.meshes) {
        if (mesh.numQuads == 0) {
            continue;
        }

        std::size_t vertexIdx = 0;
        for (int i = 0; i < mesh.numQuads; ++i, vertexIdx += 4) {
            const auto& v0 = mesh.vertices[vertexIdx + 0];
            const auto& v1 = mesh.vertices[vertexIdx + 1];
            const auto& v2 = mesh.vertices[vertexIdx + 2];
            const auto& v3 = mesh.vertices[vertexIdx + 3];

            // draw quad
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
            psyqo::GTE::Kernels::rtpt();

            using TexQuadGouraudPrim =
                psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedQuad>;
            auto& quadFrag = *(TexQuadGouraudPrim*)nextpri;
            auto& quad2d = quadFrag.primitive;
            quad2d.uvA.u = v0.uv.vx;
            quad2d.uvA.v = v0.uv.vy;
            quad2d.uvB.u = v1.uv.vx;
            quad2d.uvB.v = v1.uv.vy;
            quad2d.uvC.u = v2.uv.vx;
            quad2d.uvC.v = v2.uv.vy;
            quad2d.uvD.u = v3.uv.vx;
            quad2d.uvD.v = v3.uv.vy;

            // TODO: read from arg
            quad2d.tpage.setPageX(5).setPageY(0).setDithering(true).set(
                psyqo::Prim::TPageAttr::Tex8Bits);
            quad2d.clutIndex = {{.x = 0, .y = 240}};

            psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SZ1>(reinterpret_cast<uint32_t*>(&zValues[0]));
            psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointB.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SZ2>(reinterpret_cast<uint32_t*>(&zValues[1]));
            psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointC.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SZ3>(reinterpret_cast<uint32_t*>(&zValues[2]));

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
            psyqo::GTE::Kernels::rtps();

            psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SZ3>(reinterpret_cast<uint32_t*>(&zValues[3]));

            const auto avgZ = (zValues[0] + zValues[1] + zValues[2] + zValues[3]) / 4;
            if (avgZ >= 0 && avgZ < 4096) {
                quad2d.setColorA({.r = v0.col.vx, .g = v0.col.vy, .b = v0.col.vz});
                quad2d.setColorB({.r = v1.col.vx, .g = v1.col.vy, .b = v1.col.vz});
                quad2d.setColorC({.r = v2.col.vx, .g = v2.col.vy, .b = v2.col.vz});
                quad2d.setColorD({.r = v3.col.vx, .g = v3.col.vy, .b = v3.col.vz});

                ot.insert(quadFrag, avgZ);
            }
            nextpri += sizeof(TexQuadGouraudPrim);
        }
    }

    gpu().chain(ot);

    // debug
    psyqo::Color c = {{.r = 255, .g = 255, .b = 255}};
    game.m_systemFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        c,
        "TRX: %d, TRY: %d, TRZ: %d",
        camTrans.x.raw(),
        camTrans.y.raw(),
        camTrans.z.raw());

    /* game.m_systemFont
        .chainprintf(game.gpu(), {{.x = 16, .y = 32}}, c, "RX: %d, RY: %d", m_angleX, m_angleY); */
    // game.m_romFont.print(game.gpu(), "Hello World!", {{.x = 16, .y = 64}}, c);
}

int main()
{
    return game.run();
}
