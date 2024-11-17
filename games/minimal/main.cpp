#include <common/syscalls/syscalls.h>
#include <psyqo/application.hh>
#include <psyqo/bump-allocator.h>
#include <psyqo/font.hh>
#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/scene.hh>
#include <psyqo/soft-math.hh>

#include <cstdint>

namespace
{
constexpr auto SCREEN_WIDTH = 320;
constexpr auto SCREEN_HEIGHT = 240;

struct Vertex {
    psyqo::GTE::PackedVec3 pos;
    std::int16_t pad;
    psyqo::Color col;
};

struct Mesh {
    eastl::vector<Vertex> vertices;
};

void SetFogNearFar(int a, int b, int h)
{
    // TODO: check params + add asserts?
    const auto dqa = ((-a * b / (b - a)) << 8) / h;
    const auto dqaF = eastl::clamp(dqa, -32767, 32767);
    const auto dqbF = ((b << 12) / (b - a) << 12);

    psyqo::GTE::write<psyqo::GTE::Register::DQA, psyqo::GTE::Unsafe>(dqaF);
    psyqo::GTE::write<psyqo::GTE::Register::DQB, psyqo::GTE::Safe>(dqbF);
}

template<typename PrimType>
void interpColor3(
    const psyqo::Color& c0,
    const psyqo::Color& c1,
    const psyqo::Color& c2,
    PrimType& prim)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB0, psyqo::GTE::Unsafe>(&c0.packed);
    psyqo::GTE::write<psyqo::GTE::Register::RGB1, psyqo::GTE::Unsafe>(&c1.packed);
    psyqo::GTE::write<psyqo::GTE::Register::RGB2, psyqo::GTE::Safe>(&c2.packed);
    psyqo::GTE::Kernels::dpct();
    psyqo::Color col;
    psyqo::GTE::read<psyqo::GTE::Register::RGB0>(&col.packed);
    prim.setColorA(col);
    psyqo::GTE::read<psyqo::GTE::Register::RGB1>(&col.packed);
    prim.setColorB(col);
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&col.packed);
    prim.setColorC(col);
}

psyqo::Color interpColor(const psyqo::Color& c)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(&c.packed);
    psyqo::GTE::Kernels::dpcs();
    psyqo::Color col;
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&col.packed);
    return col;
};

class Game final : public psyqo::Application {
public:
    void prepare() override;
    void createScene() override;

    static constexpr auto OT_SIZE = 1024;
    using OrderingTableType = psyqo::OrderingTable<OT_SIZE>;
    eastl::array<OrderingTableType, 2> ots;

    static constexpr int PRIMBUFFLEN = 1000;
    using PrimBufferAllocatorType = psyqo::BumpAllocator<PRIMBUFFLEN>;
    eastl::array<PrimBufferAllocatorType, 2> primBuffers;

    OrderingTableType& getOrderingTable() { return ots[gpu().getParity()]; }
    PrimBufferAllocatorType& getPrimBuffer() { return primBuffers[gpu().getParity()]; }

    psyqo::Font<> systemFont;
};

class Scene final : public psyqo::Scene {
    void start(StartReason reason) override;
    void frame() override;
    void drawMesh(Mesh& mesh);
    void drawQuadMesh(Mesh& mesh);

    Mesh mesh;
    Mesh mesh2;

    psyqo::Vec3 camPos;
    psyqo::Angle cameraAngleY;
    psyqo::Trig<> trig;
    psyqo::Matrix33 viewRot;
};

Game game;
Scene scene;
}

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
    systemFont.uploadSystemFont(gpu());
    pushScene(&scene);
}

void Scene::start(StartReason reason)
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
    const auto farColor = psyqo::Color{.r = 80, .g = 80, .b = 80};
    psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(farColor.r);
    psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(farColor.g);
    psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Unsafe>(farColor.b);

    mesh.vertices.resize(3);

    mesh.vertices[0].pos = psyqo::GTE::PackedVec3{0.01, -0.49, 2.0};
    mesh.vertices[0].col = psyqo::Color{.r = 255, .g = 0, .b = 0};

    mesh.vertices[1].pos = psyqo::GTE::PackedVec3{0.0, -0.5, 2.0};
    mesh.vertices[1].col = psyqo::Color{.r = 0, .g = 255, .b = 0};

    mesh.vertices[2].pos = psyqo::GTE::PackedVec3{0.01, -0.5, 2.0};
    mesh.vertices[2].col = psyqo::Color{.r = 0, .g = 0, .b = 255};

    mesh2.vertices.resize(4);

    mesh2.vertices[0].pos = psyqo::GTE::PackedVec3{0.01, -0.49, 2.1};
    mesh2.vertices[0].col = psyqo::Color{.r = 255, .g = 0, .b = 0};

    mesh2.vertices[1].pos = psyqo::GTE::PackedVec3{0.0, -0.5, 2.1};
    mesh2.vertices[1].col = psyqo::Color{.r = 0, .g = 255, .b = 0};

    mesh2.vertices[2].pos = psyqo::GTE::PackedVec3{0.02, -0.49, 2.1};
    mesh2.vertices[2].col = psyqo::Color{.r = 0, .g = 0, .b = 255};

    mesh2.vertices[3].pos = psyqo::GTE::PackedVec3{0.02, -0.51, 2.1};
    mesh2.vertices[3].col = psyqo::Color{.r = 255, .g = 0, .b = 255};

    camPos = psyqo::Vec3{0.f, 0.5f, -1.9f};
}

void Scene::frame()
{
    auto& ot = game.getOrderingTable();
    auto& primBuffer = game.getPrimBuffer();
    primBuffer.reset();

    camPos.z -= 0.001;

    // set dithering ON globally
    auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
    tpage.primitive.attr.setDithering(true);
    gpu().chain(tpage);

    // clear
    psyqo::Color bg{{.r = 33, .g = 14, .b = 58}};
    auto& fill = primBuffer.allocateFragment<psyqo::Prim::FastFill>();
    gpu().getNextClear(fill.primitive, bg);
    gpu().chain(fill);

    viewRot =
        psyqo::SoftMath::generateRotationMatrix33(cameraAngleY, psyqo::SoftMath::Axis::X, trig);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(viewRot);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(camPos);

    drawMesh(mesh);
    drawQuadMesh(mesh2);

    gpu().chain(ot);

    static const psyqo::Color textCol = {{.r = 255, .g = 255, .b = 255}};
    game.systemFont.chainprint(game.gpu(), "Hello world!", {{.x = 16, .y = 32}}, textCol);
    game.systemFont.chainprintf(
        game.gpu(),
        {{.x = 16, .y = 48}},
        textCol,
        "camPos = (%.2f, %.2f, %.2f)",
        camPos.x,
        camPos.y,
        camPos.z);
}

void Scene::drawMesh(Mesh& mesh)
{
    auto& ot = game.getOrderingTable();
    auto& primBuffer = game.getPrimBuffer();

    int vertexIdx = 0;
    for (int i = 0; i < mesh.vertices.size() / 3; ++i, vertexIdx += 3) {
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
        auto avgZ = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ <= 0 || avgZ >= Game::OT_SIZE) {
            continue;
        }

        auto& triFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudTriangle>();
        auto& tri2d = triFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

        tri2d.interpolateColors(&v0.col, &v1.col, &v2.col);

        ot.insert(triFrag, avgZ);
    }
}

void Scene::drawQuadMesh(Mesh& mesh)
{
    auto& ot = game.getOrderingTable();
    auto& primBuffer = game.getPrimBuffer();

    int vertexIdx = 0;
    for (int i = 0; i < mesh.vertices.size() / 4; ++i, vertexIdx += 4) {
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
        if (dot <= 0) {
            continue;
        }

        auto& quadFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
        auto& quad2d = quadFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::Kernels::avsz4();
        auto avgZ = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ <= 0 || avgZ >= Game::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

        // TEMP: psyqo's interpolateColors is broken
        quad2d.interpolateColors(&v0.col, &v1.col, &v2.col, &v3.col);
        /* interpColor3(v0.col, v1.col, v2.col, quad2d);
        quad2d.setColorD(interpColor(v3.col)); */

        ot.insert(quadFrag, avgZ);
    }
}

int main()
{
    return game.run();
}
