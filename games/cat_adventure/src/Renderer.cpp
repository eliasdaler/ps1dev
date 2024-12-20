#include "Renderer.h"

#include "Camera.h"
#include "Common.h"
#include "Object.h"

#include <psyqo/soft-math.hh>

#include "gte-math.h"

#include <common/syscalls/syscalls.h>

#include <psyqo/primitives/lines.hh>

namespace
{

template<typename PrimType>
void interpColorD(const psyqo::Color& c, PrimType& prim)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(&c.packed);
    psyqo::GTE::Kernels::dpcs();
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&prim.colorD.packed);
};

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
    col.packed &= 0xffffff;
    prim.setColorA(col);

    psyqo::GTE::read<psyqo::GTE::Register::RGB1>(&prim.colorB.packed);
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&prim.colorC.packed);
}

void interpColor(const psyqo::Color& input, uint32_t p, psyqo::Color* out)
{
    auto max = 0x1FFFF;
    if (p > max) {
        p = max;
    }
    psyqo::GTE::write<psyqo::GTE::Register::IR0>(&p);
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(&input.packed);
    psyqo::GTE::Kernels::dpcs();
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&out->packed);
}

}

Renderer::Renderer(psyqo::GPU& gpu) : gpu(gpu)
{}

void Renderer::init()
{
    // screen "center" (screenWidth / 2, screenHeight / 2)
    psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_WIDTH / 2.0).raw());
    psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_HEIGHT / 2.0).raw());

    // projection plane distance
    setFOV(250);

    // FIXME: use OT_SIZE here somehow?
    psyqo::GTE::write<psyqo::GTE::Register::ZSF3, psyqo::GTE::Unsafe>(1024 / 3);
    psyqo::GTE::write<psyqo::GTE::Register::ZSF4, psyqo::GTE::Unsafe>(1024 / 4);
}

void Renderer::calculateViewModelMatrix(const Object& object, const Camera& camera, bool setViewRot)
{
    if (setViewRot) {
        // V * M
        psyqo::Matrix33 viewModelMatrix;
        psyqo::GTE::Math::multiplyMatrix33<
            psyqo::GTE::PseudoRegister::Rotation,
            psyqo::GTE::PseudoRegister::
                V0>(camera.viewRot, object.transform.rotation, &viewModelMatrix);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(viewModelMatrix);
    }

    // what 4th column would be if we did V * M
    auto posCamSpace = object.transform.translation - camera.position;
    if (!setViewRot) {
        psyqo::GTE::Math::matrixVecMul3<
            psyqo::GTE::PseudoRegister::Rotation,
            psyqo::GTE::PseudoRegister::V0>(posCamSpace, &posCamSpace);
    } else {
        // TODO: use L?
        psyqo::SoftMath::matrixVecMul3(camera.viewRot, posCamSpace, &posCamSpace);
    }
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);
}

bool Renderer::shouldCullObject(const Object& object, const Camera& camera) const
{
    static constexpr auto cullDistance = psyqo::FixedPoint<>(64.f);
    auto posCamSpace = object.transform.translation - camera.position;
    if (posCamSpace.x * posCamSpace.x + posCamSpace.y * posCamSpace.y +
            posCamSpace.z * posCamSpace.z >
        cullDistance) {
        return true;
    }
    return false;
}

void Renderer::drawModelObject(
    const ModelObject& object,
    const Camera& camera,
    const TextureInfo& texture,
    bool setViewRot)
{
    if (shouldCullObject(object, camera)) {
        return;
    }
    calculateViewModelMatrix(object, camera, setViewRot);
    drawModel(*object.model, texture);
}

void Renderer::drawModelObject(
    const ModelObject& object,
    const Armature& armature,
    const Camera& camera,
    const TextureInfo& texture,
    bool setViewRot)
{
    if (shouldCullObject(object, camera)) {
        return;
    }

    // V * M
    psyqo::Matrix33 vmMatrix;
    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(camera.viewRot, object.transform.rotation, &vmMatrix);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(vmMatrix);

    // V * M * J
    const auto& jointTransform = armature.getRootJoint().globalTransform;
    psyqo::Matrix33 vmjMatrix;
    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(jointTransform.rotation, &vmjMatrix);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(vmjMatrix);

    auto jtModelSpace = jointTransform.translation;
    psyqo::GTE::Math::matrixVecMul3<
        psyqo::GTE::PseudoRegister::Light,
        psyqo::GTE::PseudoRegister::V0>(jointTransform.rotation, jtModelSpace, &jtModelSpace);

    // what 4th column would be if we did V * M * J
    auto posCamSpace = jtModelSpace + object.transform.translation - camera.position;
    psyqo::GTE::Math::matrixVecMul3<
        psyqo::GTE::PseudoRegister::Light,
        psyqo::GTE::PseudoRegister::V0>(vmMatrix, posCamSpace, &posCamSpace);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

    const auto& model = *object.model;
    for (const auto& mesh : model.meshes) {
        drawMesh(mesh, texture);
    }
}

void Renderer::drawMeshObject(
    const MeshObject& object,
    const Camera& camera,
    const TextureInfo& texture)
{
    if (shouldCullObject(object, camera)) {
        return;
    }
    calculateViewModelMatrix(object, camera, false);
    drawMesh(*object.mesh, texture);
}

void Renderer::drawModel(const Model& model, const TextureInfo& texture)
{
    for (const auto& mesh : model.meshes) {
        drawMesh(mesh, texture);
    }
}

void Renderer::drawMesh(const Mesh& mesh, const TextureInfo& texture)
{
    std::size_t vertexIdx = 0;
    drawTris<psyqo::Prim::GouraudTriangle>(mesh, texture, mesh.numUntexturedTris, vertexIdx);
    drawQuads<psyqo::Prim::GouraudQuad>(mesh, texture, mesh.numUntexturedQuads, vertexIdx);
    drawTris<psyqo::Prim::GouraudTexturedTriangle>(mesh, texture, mesh.numTris, vertexIdx);
    drawQuads<psyqo::Prim::GouraudTexturedQuad>(mesh, texture, mesh.numQuads, vertexIdx);

    gpu.pumpCallbacks();
}

template<typename PrimType>
void Renderer::drawTris(
    const Mesh& mesh,
    const TextureInfo& texture,
    int numFaces,
    std::size_t& outVertIdx)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();
    auto vertexIdx = outVertIdx;

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

        auto avgZ = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += bias; // add bias
        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        auto& triFrag = primBuffer.allocateFragment<PrimType>();
        auto& tri2d = triFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

        const auto sz0 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
        const auto sz1 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ2>();
        const auto sz2 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ3>();

        { // per vertex interpolation
            const auto p0 = calcInterpFactor(sz0);
            const auto p1 = calcInterpFactor(sz1);
            const auto p2 = calcInterpFactor(sz2);

            psyqo::Color col;
            interpColor(v0.col, p0, &col);
            tri2d.setColorA(col);
            interpColor(v1.col, p1, &tri2d.colorB);
            interpColor(v2.col, p2, &tri2d.colorC);
        }

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

    outVertIdx = vertexIdx;
}

template<typename PrimType>
void Renderer::drawQuads(
    const Mesh& mesh,
    const TextureInfo& texture,
    int numFaces,
    std::size_t& outVertIdx)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();
    auto vertexIdx = outVertIdx;

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

        auto avgZ = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += bias; // add bias
        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

        { // per vertex interpolation
            const auto sz0 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ0>();
            const auto sz1 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
            const auto sz2 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ2>();
            const auto sz3 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ3>();

            const auto p0 = calcInterpFactor(sz0);
            const auto p1 = calcInterpFactor(sz1);
            const auto p2 = calcInterpFactor(sz2);
            const auto p3 = calcInterpFactor(sz3);

            psyqo::Color col;
            interpColor(v0.col, p0, &col);
            quad2d.setColorA(col);
            interpColor(v1.col, p1, &quad2d.colorB);
            interpColor(v2.col, p2, &quad2d.colorC);
            interpColor(v3.col, p3, &quad2d.colorD);
        }

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

    outVertIdx = vertexIdx;
}

void Renderer::drawObjectAxes(const Object& object, const Camera& camera)
{
    calculateViewModelMatrix(object, camera, true);

    constexpr auto axisLength = psyqo::FixedPoint<>(0.025f);
    drawLineLocalSpace({}, {axisLength, 0.f, 0.f}, {.r = 255, .g = 0, .b = 0});
    drawLineLocalSpace({}, {0.f, axisLength, 0.f}, {.r = 0, .g = 255, .b = 0});
    drawLineLocalSpace({}, {0.f, 0.f, axisLength}, {.r = 0, .g = 0, .b = 255});
}

void Renderer::drawLineLocalSpace(const psyqo::Vec3& a, const psyqo::Vec3& b, const psyqo::Color& c)
{
    auto& primBuffer = getPrimBuffer();

    auto& lineFrag = primBuffer.allocateFragment<psyqo::Prim::Line>();
    auto& line = lineFrag.primitive;
    line.setColor(c);

    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(a);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(b);
    psyqo::GTE::Kernels::rtpt();

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&line.pointA.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&line.pointB.packed);

    gpu.chain(lineFrag);
}

void Renderer::drawLineWorldSpace(
    const Camera& camera,
    const psyqo::Vec3& a,
    const psyqo::Vec3& b,
    const psyqo::Color& c)
{
    auto& primBuffer = getPrimBuffer();

    // TODO: allow to not do this over and over?
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);

    // what 4th column would be if we did V * M
    auto posCamSpace = a - camera.position;
    psyqo::SoftMath::matrixVecMul3(camera.viewRot, posCamSpace, &posCamSpace);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

    auto& lineFrag = primBuffer.allocateFragment<psyqo::Prim::Line>();
    auto& line = lineFrag.primitive;
    line.setColor(c);

    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(psyqo::Vec3{}); // a is origin
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(b - a); // b relative to a
    psyqo::GTE::Kernels::rtpt();

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&line.pointA.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&line.pointB.packed);

    gpu.chain(lineFrag);
}

void Renderer::setFogNearFar(psyqo::FixedPoint<> near, psyqo::FixedPoint<> far)
{
    const auto a = near.value;
    const auto b = far.value;
    const auto h = SCREEN_WIDTH / 2;

    // TODO: rewrite this to use fixed point numbers directly
    const auto dqa = ((-a * b / (b - a)) << 8) / h;
    const auto dqaF = eastl::clamp(dqa, -32767, 32767);
    const auto dqbF = ((b << 12) / (b - a) << 12);

    psyqo::GTE::write<psyqo::GTE::Register::DQA, psyqo::GTE::Unsafe>(dqaF);
    psyqo::GTE::write<psyqo::GTE::Register::DQB, psyqo::GTE::Safe>(dqbF);

    this->dqa = dqaF;
    this->dqb = dqbF;
}

void Renderer::setFarColor(const psyqo::Color& c)
{
    psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(c.r);
    psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(c.g);
    psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Safe>(c.b);
}

uint32_t Renderer::calcInterpFactor(uint32_t sz)
{
    // this is what GTE does when rtpt/rtps
    if (sz == 0) {
        sz = 1;
    }

    const auto mac0 = (((h * 0x20000) / sz + 1) / 2) * dqa + dqb;
    return mac0 / 0x1000;
}

void Renderer::setFOV(uint32_t nh)
{
    h = nh;
    psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(250);
}
