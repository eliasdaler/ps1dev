#include "Renderer.h"

#include "Camera.h"
#include "Object.h"

#include <psyqo/soft-math.hh>

#include "gte-math.h"

namespace
{

psyqo::Color interpColor(const psyqo::Color& c)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(&c.packed);
    psyqo::GTE::Kernels::dpcs();
    psyqo::Color col;
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&col.packed);
    return col;
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
    prim.setColorA(col);
    psyqo::GTE::read<psyqo::GTE::Register::RGB1>(&col.packed);
    prim.setColorB(col);
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&col.packed);
    prim.setColorC(col);
}
}

Renderer::Renderer(psyqo::GPU& gpu) : gpu(gpu)
{}

void Renderer::drawModelObject(
    const ModelObject& object,
    const Camera& camera,
    const TextureInfo& texture)
{
    if (object.rotation.x == 0.0 && object.rotation.y == 0.0) {
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camera.viewRot);
    } else {
        // yaw
        auto objectRotMat = psyqo::SoftMath::
            generateRotationMatrix33(object.rotation.y, psyqo::SoftMath::Axis::Y, trig);

        if (object.rotation.x != 0.0) { // pitch
            const auto rotX = psyqo::SoftMath::
                generateRotationMatrix33(object.rotation.x, psyqo::SoftMath::Axis::X, trig);
            // psyqo::SoftMath::multiplyMatrix33(objectRotMat, rotX, &objectRotMat);
            psyqo::GTE::Math::multiplyMatrix33<
                psyqo::GTE::PseudoRegister::Rotation,
                psyqo::GTE::PseudoRegister::V0>(objectRotMat, rotX, &objectRotMat);
        }

        // psyqo::SoftMath::multiplyMatrix33(camera.viewRot, objectRotMat, &objectRotMat);
        psyqo::GTE::Math::multiplyMatrix33<
            psyqo::GTE::PseudoRegister::Rotation,
            psyqo::GTE::PseudoRegister::V0>(camera.viewRot, objectRotMat, &objectRotMat);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(objectRotMat);
    }

    auto posCamSpace = object.position;
    // Note: can't use Rotation matrix here as objectRotMat is currently uploaded there
    // psyqo::SoftMath::matrixVecMul3(camera.viewRot, posCamSpace, &posCamSpace);
    psyqo::GTE::Math::matrixVecMul3<
        psyqo::GTE::PseudoRegister::Light,
        psyqo::GTE::PseudoRegister::V0>(camera.viewRot, posCamSpace, &posCamSpace);
    posCamSpace += camera.translation;

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

    drawModel(*object.model, texture);
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
        const auto avgZ =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ < 0 || avgZ >= Renderer::OT_SIZE) {
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
        const auto avgZ =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ < 0 || avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

        // TEMP: psyqo's interpolateColors is broken
        // quad2d.interpolateColors(&v0.col, &v1.col, &v2.col, &v3.col);
        interpColor3(v0.col, v1.col, v2.col, quad2d);
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

    outVertIdx = vertexIdx;
}