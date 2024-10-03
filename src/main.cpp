#include <psyqo/application.hh>
#include <psyqo/font.hh>
#include <psyqo/gpu.hh>
#include <psyqo/scene.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/soft-math.hh>

#include <psyqo/cdrom-device.hh>
#include <psyqo/iso9660-parser.hh>
#include <psyqo-paths/cdrom-loader.hh>

#include <common/syscalls/syscalls.h>

#include <cstdint>

#include "TimFile.h"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

namespace
{

class Game final : public psyqo::Application {
    void prepare() override;
    void createScene() override;

    void uploadTIM();

public:
    psyqo::Font<> m_systemFont;
    psyqo::Font<> m_romFont;
    psyqo::Trig<> m_trig;

    psyqo::CDRomDevice m_cdrom;
    psyqo::ISO9660Parser m_isoParser = psyqo::ISO9660Parser(&m_cdrom);
    psyqo::paths::CDRomLoader m_cdromLoader;
    eastl::vector<uint8_t> m_buffer;
};

struct Quad {
    psyqo::GTE::PackedVec3 verts[4];
};

using Short = psyqo::GTE::Short;

// And we need at least one scene to be created.
// This is the one we're going to do for our hello world.
class GameplayScene final : public psyqo::Scene {
    void start(StartReason reason) override
    {
        const auto raw = Short::Raw::RAW;

#define SHORT(x) Short((x), raw)

        quad3d.verts[0] = psyqo::GTE::PackedVec3(SHORT(-256), SHORT(-512), SHORT(768));
        quad3d.verts[1] = psyqo::GTE::PackedVec3(SHORT(256), SHORT(-512), SHORT(768));
        quad3d.verts[2] = psyqo::GTE::PackedVec3(SHORT(-256), SHORT(0), SHORT(768));
        quad3d.verts[3] = psyqo::GTE::PackedVec3(SHORT(256), SHORT(0), SHORT(768));

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
    Quad quad3d;

    psyqo::Vec3 camPos{-500.f, 0.f, -1000.f};
    psyqo::Vec3 camRot{200.f, 230.f, 0.f};

    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::TexturedQuad>, 1> m_quads[2];
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> fills[2];

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

void Game::createScene()
{
    m_systemFont.uploadSystemFont(gpu());
    m_romFont.uploadKromFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});
    pushScene(&gameplayScene);

    // load stuff from CD
    m_cdromLoader
        .readFile("BRICKS.TIM;1", gpu(), m_isoParser, [this](eastl::vector<uint8_t>&& buffer) {
            m_buffer = eastl::move(buffer);
            uploadTIM();
        });
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

    ramsyscall_printf("TIM loaded! Num bytes: %d\n", m_buffer.size());
}

void GameplayScene::frame()
{
    // calculate camera rotation matrix
    psyqo::Angle m_angleX;
    psyqo::Angle m_angleY;
    m_angleX.value = (camRot.x.value / 2) >> 12;
    m_angleY.value = (camRot.y.value / 2) >> 12;
    auto rotX =
        psyqo::SoftMath::generateRotationMatrix33(m_angleX, psyqo::SoftMath::Axis::X, &game.m_trig);
    const auto rotY =
        psyqo::SoftMath::generateRotationMatrix33(m_angleY, psyqo::SoftMath::Axis::Y, &game.m_trig);
    psyqo::SoftMath::multiplyMatrix33(&rotY, &rotX, &rotX);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(rotX);

    // calcuolate and upload camera translation
    psyqo::Vec3 camTrans{};
    camTrans.x = -camPos.x >> 12;
    camTrans.y = -camPos.y >> 12;
    camTrans.z = -camPos.z >> 12;

    psyqo::SoftMath::matrixVecMul3(&rotX, &camTrans, &camTrans);

    psyqo::GTE::write<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>(camTrans.x.raw());
    psyqo::GTE::write<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>(camTrans.y.raw());
    psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>(camTrans.z.raw());

    const auto parity = gpu().getParity();

    // draw
    psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
    gpu().getNextClear(fills[parity].primitive, bg);
    // game.gpu().clear(bg);

    auto& ot = ots[parity];

    // draw quad
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(quad3d.verts[0]);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(quad3d.verts[1]);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(quad3d.verts[2]);
    psyqo::GTE::Kernels::rtpt();

    auto& quads = m_quads[parity];
    auto& quad2d = quads[0].primitive;
    // set quad2d const stuff
    auto u0 = 0;
    auto v0 = 64;
    auto u1 = 63;
    auto v1 = 127;
    quad2d.uvA.u = u0;
    quad2d.uvA.v = v0;
    quad2d.uvB.u = u1;
    quad2d.uvB.v = v0;
    quad2d.uvC.u = u0;
    quad2d.uvC.v = v1;
    quad2d.uvD.u = u1;
    quad2d.uvD.v = v1;
    quad2d.tpage.setPageX(5).setPageY(0).setDithering(true).set(psyqo::Prim::TPageAttr::Tex8Bits);
    quad2d.clutIndex = {{.x = 0, .y = 240}};
    quad2d.setColor({.r = 127, .g = 127, .b = 127});

    int32_t zValues[4];
    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SZ1>(reinterpret_cast<uint32_t*>(&zValues[0]));
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointB.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SZ2>(reinterpret_cast<uint32_t*>(&zValues[1]));
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointC.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SZ3>(reinterpret_cast<uint32_t*>(&zValues[2]));

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(quad3d.verts[3]);
    psyqo::GTE::Kernels::rtps();

    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SZ3>(reinterpret_cast<uint32_t*>(&zValues[3]));

    const auto avgZ = (zValues[0] + zValues[1] + zValues[2] + zValues[3]) / 4;
    if (avgZ >= 0 && avgZ < 4096) {
        ot.insert(quads[0], avgZ);
    }
    gpu().chain(ot);
    // gpu().sendPrimitive(quad2d);

    // debug
    /*
    psyqo::Color c = {{.r = 255, .g = 255, .b = 255}};
    game.m_systemFont.printf(
        game.gpu(),
        {{.x = 16, .y = 16}},
        c,
        "TRX: %d, TRY: %d, TRZ: %d",
        camTrans.x.raw(),
        camTrans.y.raw(),
        camTrans.z.raw());
    game.m_systemFont
        .printf(game.gpu(), {{.x = 16, .y = 32}}, c, "RX: %d, RY: %d", m_angleX, m_angleY); */
    // game.m_romFont.print(game.gpu(), "Hello World!", {{.x = 16, .y = 64}}, c);
}

int main()
{
    return game.run();
}
