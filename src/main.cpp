#include <psyqo/application.hh>
#include <psyqo/font.hh>
#include <psyqo/gpu.hh>
#include <psyqo/scene.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/soft-math.hh>

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

namespace
{

// A PSYQo software needs to declare one `Application` object.
// This is the one we're going to do for our hello world.
class Game final : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    psyqo::Font<> m_systemFont;
    psyqo::Font<> m_romFont;
    psyqo::Trig<> m_trig;
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
        static constexpr psyqo::GTE::Short tileSize{1.0};
        auto raw = Short::Raw::RAW;
#define SHORT(x) Short((x), raw)

        quad3d.verts[0] = psyqo::GTE::PackedVec3(SHORT(-256), SHORT(-512), SHORT(768));
        quad3d.verts[1] = psyqo::GTE::PackedVec3(SHORT(256), SHORT(-512), SHORT(768));
        quad3d.verts[2] = psyqo::GTE::PackedVec3(SHORT(-256), SHORT(0), SHORT(768));
        quad3d.verts[3] = psyqo::GTE::PackedVec3(SHORT(256), SHORT(0), SHORT(768));

        psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(h);

        psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(
            psyqo::FixedPoint<16>(160.0).raw());
        psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(
            psyqo::FixedPoint<16>(120.0).raw());
    }

    void frame() override;

    psyqo::Prim::Quad quad2d{{.r = 255, .g = 0, .b = 255}};
    psyqo::OrderingTable<4096> ots[2];
    Quad quad3d;

    psyqo::Vec3 camPos{-500.f, 0.f, -1000.f};
    psyqo::Vec3 camRot{200.f, 230.f, 0.f};

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
}

void Game::createScene()
{
    // We're going to use two fonts, one from the system, and one from the kernel rom.
    // We need to upload them to VRAM first. The system font is 256x48x4bpp, and the
    // kernel rom font is 256x90x4bpp. We're going to upload them to the same texture
    // page, so we need to make sure they don't overlap. The default location for the
    // system font is {{.x = 960, .y = 464}}, and the default location for the kernel
    // rom font is {{.x = 960, .y = 422}}, so we need to nudge the kernel rom
    // font up a bit.
    m_systemFont.uploadSystemFont(gpu());
    m_romFont.uploadKromFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});
    pushScene(&gameplayScene);
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

    // draw
    psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
    game.gpu().clear(bg);

    const auto parity = gpu().getParity();
    auto& ot = ots[parity];

    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(quad3d.verts[0]);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(quad3d.verts[1]);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(quad3d.verts[2]);
    psyqo::GTE::Kernels::rtpt();

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointB.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointC.packed);

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(quad3d.verts[3]);
    psyqo::GTE::Kernels::rtps();
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

    gpu().sendPrimitive(quad2d);

    // debug
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
        .printf(game.gpu(), {{.x = 16, .y = 32}}, c, "RX: %d, RY: %d", m_angleX, m_angleY);
    // game.m_romFont.print(game.gpu(), "Hello World!", {{.x = 16, .y = 64}}, c);
}

int main()
{
    return game.run();
}
