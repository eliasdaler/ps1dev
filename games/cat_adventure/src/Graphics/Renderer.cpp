#include "Renderer.h"

#include <common/syscalls/syscalls.h>
#include <psyqo/primitives/lines.hh>
#include <psyqo/soft-math.hh>

#include <Camera.h>
#include <Common.h>
#include <Graphics/TimFile.h>
#include <Math/gte-math.h>
#include <Object.h>

#include <cstring> // memcpy

#include "Subdivision.h"

namespace
{
void interpColor(const psyqo::Color& input, uint32_t p, psyqo::Color* out)
{
    const auto max = 0x1FFFF;
    if (p > max) {
        p = max;
    }
    psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(&p);
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(&input.packed);
    psyqo::GTE::Kernels::dpcs();
    psyqo::GTE::read<psyqo::GTE::Register::RGB2>(&out->packed);
}

// Interpolate color immediately after doing rtps (IR0 has "p" in it already)
psyqo::Color interpColorImm(psyqo::Color input)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(input.packed);
    psyqo::GTE::Kernels::dpcs();
    return {.packed = psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2>()};
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
                V0>(camera.view.rotation, object.transform.rotation, &viewModelMatrix);
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
        psyqo::SoftMath::matrixVecMul3(camera.view.rotation, posCamSpace, &posCamSpace);
    }
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);
}

bool Renderer::shouldCullObject(const Object& object, const Camera& camera) const
{
    static constexpr auto cullDistance = psyqo::FixedPoint<>(30.f);
    auto posCamSpace = object.transform.translation - camera.position;
    if (posCamSpace.x * posCamSpace.x + posCamSpace.y * posCamSpace.y +
            posCamSpace.z * posCamSpace.z >
        cullDistance) {
        return true;
    }
    return false;
}

void Renderer::drawModelObject(ModelObject& object, const Camera& camera, bool setViewRot)
{
    if (shouldCullObject(object, camera)) {
        return;
    }
    calculateViewModelMatrix(object, camera, setViewRot);

    drawModel(object.model);
}

void Renderer::drawAnimatedModelObject(
    AnimatedModelObject& object,
    const Camera& camera,
    bool setViewRot)
{
    if (shouldCullObject(object, camera)) {
        return;
    }

    auto& model = object.model;
    const auto& armature = model.armature;

    if (armature.joints.empty()) {
        drawModelObject(object, camera);
        return;
    }

    for (auto& mesh : model.meshes) {
        drawMeshArmature(object, camera, armature, mesh);
    }
}

void Renderer::drawMeshArmature(
    const AnimatedModelObject& object,
    const Camera& camera,
    const Armature& armature,
    const Mesh& mesh)
{
    const auto& meshData = *mesh.meshData;
    const auto& jointTransform = object.jointGlobalTransforms[meshData.jointId];

    const auto t1 = combineTransforms(object.transform, jointTransform);

    // This is basically combineTransforms(camera.view, t1),
    // but we do a trick which allows us to prevent 4.12 range overflow
    {
        using namespace psyqo::GTE;
        using namespace psyqo::GTE::Math;

        TransformMatrix t2;

        // V * M * J
        multiplyMatrix33<
            PseudoRegister::Rotation,
            PseudoRegister::V0>(camera.view.rotation, t1.rotation, &t2.rotation);

        // Instead of using camera.view.translation (which is V * (-camPos)),
        // we're going into the camera/view space and do calculations there
        t2.translation = t1.translation - camera.position;
        matrixVecMul3<
            PseudoRegister::Rotation, // camera.view.rotation
            PseudoRegister::V0>(t2.translation, &t2.translation);

        writeSafe<PseudoRegister::Rotation>(t2.rotation);
        writeSafe<PseudoRegister::Translation>(t2.translation);
    }

    drawMesh(mesh);
}

void Renderer::drawMeshObject(MeshObject& object, const Camera& camera)
{
    if (shouldCullObject(object, camera)) {
        return;
    }
    calculateViewModelMatrix(object, camera, false);
    drawMesh(object.mesh);
}

void Renderer::drawModel(const Model& model)
{
    for (const auto& mesh : model.meshes) {
        drawMesh(mesh);
    }
}

void Renderer::drawQuadSubdiv(const psyqo::Prim::GouraudTexturedQuad& prim, int avgZ, int addBias)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    auto& wrk1 = *(SubdivData1*)(SCRATCH_PAD);

    /* verts set in drawMesh */

    wrk1.ouv[0].u = prim.uvA.u;
    wrk1.ouv[0].v = prim.uvA.v;

    wrk1.ouv[1].u = prim.uvB.u;
    wrk1.ouv[1].v = prim.uvB.v;

    wrk1.ouv[2].u = prim.uvC.u;
    wrk1.ouv[2].v = prim.uvC.v;

    wrk1.ouv[3].u = prim.uvD.u;
    wrk1.ouv[3].v = prim.uvD.v;

    wrk1.ocol[0] = prim.colorA();
    wrk1.ocol[1] = prim.colorB;
    wrk1.ocol[2] = prim.colorC;
    wrk1.ocol[3] = prim.colorD;

    const auto tpage = prim.tpage;
    const auto clut = prim.clutIndex;

    if (avgZ < LEVEL_2_SUBDIV_DIST) {
        auto& wrk2 = *(SubdivData2*)(SCRATCH_PAD);
        for (int i = 0; i < 4; ++i) {
            wrk2.oov[i] = wrk2.ov[i];
            wrk2.oouv[i] = wrk2.ouv[i];
            wrk2.oocol[i] = wrk2.ocol[i];
        }

        DRAW_QUADS_44(wrk2);
        return;
    }

    DRAW_QUADS_22(wrk1);
}

void Renderer::drawMesh(const Mesh& mesh)
{
    // static constexpr auto neutralColor = psyqo::Color{.r = 64 + 20, .g = 80 + 20, .b = 100 + 20};
    static constexpr auto neutralColor = psyqo::Color{.r = 64 + 40, .g = 80 + 40, .b = 100 + 40};

    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    const auto& meshData = *mesh.meshData;
    const auto& g3s = meshData.g3;
    const auto& g4s = meshData.g4;
    const auto& gt3s = meshData.gt3;
    const auto& gt4s = meshData.gt4;

    const auto g4Offset = g3s.size() * 3;
    const auto gt3Offset = g4Offset + g4s.size() * 4;
    const auto gt4Offset = gt3Offset + gt3s.size() * 3;

    const auto& verts = meshData.vertices;

    /* for (std::size_t i = 0; i < g3s.size(); ++i) {
        const auto& v0 = verts[i * 3 + 0];
        const auto& v1 = verts[i * 3 + 1];
        const auto& v2 = verts[i * 3 + 2];

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

        auto& triFrag = g3s[i];
        auto& tri2d = triFrag.primitive;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

        if (fogEnabled) {
            const auto sz0 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
            const auto sz1 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ2>();
            const auto sz2 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ3>();

            // per vertex interpolation
            const auto p0 = calcInterpFactor(sz0);
            const auto p1 = calcInterpFactor(sz1);
            const auto p2 = calcInterpFactor(sz2);

            psyqo::Color col;
            interpColor(neutralColor, p0, &col);
            tri2d.setColorA(col);
            interpColor(neutralColor, p1, &tri2d.colorB);
            interpColor(neutralColor, p2, &tri2d.colorC);
        }

        ot.insert(triFrag, avgZ);
    }

    for (std::size_t i = 0; i < g4s.size(); ++i) {
        const auto& v0 = verts[g4Offset + i * 4 + 0];
        const auto& v1 = verts[g4Offset + i * 4 + 1];
        const auto& v2 = verts[g4Offset + i * 4 + 2];
        const auto& v3 = verts[g4Offset + i * 4 + 3];

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

        auto& quadFrag = g4s[i];
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

        if (fogEnabled) { // per vertex interpolation
            const auto sz0 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ0>();
            const auto sz1 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
            const auto sz2 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ2>();
            const auto sz3 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ3>();

            const auto p0 = calcInterpFactor(sz0);
            const auto p1 = calcInterpFactor(sz1);
            const auto p2 = calcInterpFactor(sz2);
            const auto p3 = calcInterpFactor(sz3);

            psyqo::Color col;
            interpColor(neutralColor, p0, &col);
            quad2d.setColorA(col);
            interpColor(neutralColor, p1, &quad2d.colorB);
            interpColor(neutralColor, p2, &quad2d.colorC);
            interpColor(neutralColor, p3, &quad2d.colorD);
        }

        ot.insert(quadFrag, avgZ);
    } */

    /* ramsyscall_printf(
        "%d, %d, %d, %d\n",
        (int)meshData.vertices.size(),
        (int)gt3s.size(),
        (int)gt4s.size(),
        (int)primBuffer.getNumBytesUsed()); */

    for (std::size_t i = 0; i < gt3s.size(); ++i) {
        auto& triFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
        auto& tri2d = triFrag.primitive;

        const auto& prim = gt3s[i];
        tri2d = prim; // copy

        const auto& v0 = verts[gt3Offset + i * 3 + 0];
        const auto& v1 = verts[gt3Offset + i * 3 + 1];
        const auto& v2 = verts[gt3Offset + i * 3 + 2];

        if (fogEnabled) {
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
            psyqo::GTE::Kernels::rtps();
            tri2d.setColorA(interpColorImm(prim.colorA()));

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
            psyqo::GTE::Kernels::rtps();
            const auto p1 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            tri2d.colorB = interpColorImm(prim.colorB);

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
            psyqo::GTE::Kernels::rtps();
            const auto p2 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            tri2d.colorC = interpColorImm(prim.colorC);
        } else {
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
            psyqo::GTE::Kernels::rtpt();
        }

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
        { // load additional bias stored in padding
            uint32_t uvCPacked = reinterpret_cast<std::uint32_t&>(tri2d.uvC);
            const auto addBias = static_cast<std::int16_t>((uvCPacked & 0xFFFF0000) >> 16);
            avgZ += addBias;
        }

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

        ot.insert(triFrag, avgZ);
    }

    for (std::size_t i = 0; i < gt4s.size(); ++i) {
        const auto& v0 = verts[gt4Offset + i * 4 + 0];
        const auto& v1 = verts[gt4Offset + i * 4 + 1];
        const auto& v2 = verts[gt4Offset + i * 4 + 2];
        const auto& v3 = verts[gt4Offset + i * 4 + 3];

        const auto& prim = gt4s[i];

        auto& quadFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        auto& quad2d = quadFrag.primitive;

        /* quad2d.command = prim.command;
        quad2d.tpage = prim.tpage;
        quad2d.clutIndex = prim.clutIndex;
        quad2d.uvA = prim.uvA;
        quad2d.uvB = prim.uvB;
        quad2d.uvC = prim.uvC;
        quad2d.uvD = prim.uvD; */
        quad2d = prim; // copy

        if (fogEnabled) {
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
            psyqo::GTE::Kernels::rtps();
            quad2d.setColorA(interpColorImm(prim.colorA()));

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
            psyqo::GTE::Kernels::rtps();
            quad2d.colorB = interpColorImm(prim.colorB);

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
            psyqo::GTE::Kernels::rtps();
            quad2d.colorC = interpColorImm(prim.colorC);
        } else {
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
            psyqo::GTE::Kernels::rtpt();
        }

        // load additional bias stored in padding
        uint32_t uvCPacked = reinterpret_cast<const std::uint32_t&>(prim.uvC);
        const auto addBias = static_cast<std::int16_t>((uvCPacked & 0xFFFF0000) >> 16);

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            if (addBias != 2) {
                continue;
            }
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();
        if (fogEnabled) {
            quad2d.colorD = interpColorImm(prim.colorD);
        }

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        // subdiv
        if ((addBias == 1 || addBias == 500) && avgZ < LEVEL_1_SUBDIV_DIST) {
            auto& wrk1 = *(SubdivData1*)(SCRATCH_PAD);
            wrk1.ov[0] = v0;
            wrk1.ov[1] = v1;
            wrk1.ov[2] = v2;
            wrk1.ov[3] = v3;

#ifdef FULL_INLINE
            wrk1.ouv[0].u = prim.uvA.u;
            wrk1.ouv[0].v = prim.uvA.v;

            wrk1.ouv[1].u = prim.uvB.u;
            wrk1.ouv[1].v = prim.uvB.v;

            wrk1.ouv[2].u = prim.uvC.u;
            wrk1.ouv[2].v = prim.uvC.v;

            wrk1.ouv[3].u = prim.uvD.u;
            wrk1.ouv[3].v = prim.uvD.v;

            wrk1.ocol[0] = quad2d.colorA();
            wrk1.ocol[1] = quad2d.colorB;
            wrk1.ocol[2] = quad2d.colorC;
            wrk1.ocol[3] = quad2d.colorD;

            const auto tpage = quad2d.tpage;
            const auto clut = quad2d.clutIndex;
            const auto command = quad2d.command;

            if (avgZ < LEVEL_2_SUBDIV_DIST) {
                auto& wrk2 = *(SubdivData2*)(SCRATCH_PAD);
                for (int i = 0; i < 4; ++i) {
                    wrk2.oov[i] = wrk2.ov[i];
                    wrk2.oouv[i] = wrk2.ouv[i];
                    wrk2.oocol[i] = wrk2.ocol[i];
                }

                DRAW_QUADS_44(wrk2);
                continue;
            }

            DRAW_QUADS_22(wrk1);
#else
            drawQuadSubdiv(quad2d, avgZ, addBias);
#endif
            continue;
        }

        avgZ += bias + addBias; // add bias

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

        ot.insert(quadFrag, avgZ);
    }
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
    const psyqo::Color& c,
    bool cameraViewLoaded)
{
    auto& primBuffer = getPrimBuffer();

    if (!cameraViewLoaded) {
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);
    }

    // what 4th column would be if we did V * M
    auto posCamSpace = a - camera.position;
    psyqo::SoftMath::matrixVecMul3(camera.view.rotation, posCamSpace, &posCamSpace);
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

void Renderer::drawAABB(const Camera& camera, const AABB& aabb, const psyqo::Color& c)
{
    const auto size = (aabb.max - aabb.min);

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);

    auto posCamSpace = aabb.min - camera.position;
    psyqo::GTE::Math::matrixVecMul3<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(posCamSpace, &posCamSpace);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

    // bottom plane
    drawLineLocalSpace({0.0, 0.0, 0.0}, {0.0, 0.0, size.z}, c);
    drawLineLocalSpace({0.0, 0.0, 0.0}, {size.x, 0.0, 0.0}, c);
    drawLineLocalSpace({size.x, 0.0, 0.0}, {size.x, 0.0, size.z}, c);
    drawLineLocalSpace({0.0, 0.0, size.z}, {size.x, 0.0, size.z}, c);

    // top plane
    drawLineLocalSpace({0.0, size.y, 0.0}, {0.0, size.y, size.z}, c);
    drawLineLocalSpace({0.0, size.y, 0.0}, {size.x, size.y, 0.0}, c);
    drawLineLocalSpace({size.x, size.y, 0.0}, {size.x, size.y, size.z}, c);
    drawLineLocalSpace({0.0, size.y, size.z}, {size.x, size.y, size.z}, c);

    // sides
    drawLineLocalSpace({0.0, 0.0, 0.0}, {0.0, size.y, 0.0}, c);
    drawLineLocalSpace({size.x, 0.0, 0.0}, {size.x, size.y, 0.0}, c);
    drawLineLocalSpace({0.0, 0.0, size.z}, {0.0, size.y, size.z}, c);
    drawLineLocalSpace({size.x, 0.0, size.z}, {size.x, size.y, size.z}, c);
}

void Renderer::drawCircle(const Camera& camera, const Circle& circle, const psyqo::Color& c)
{
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);

    auto posCamSpace = circle.center - camera.position;
    psyqo::GTE::Math::matrixVecMul3<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(posCamSpace, &posCamSpace);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

    static constexpr auto numSegments = 16.0;
    psyqo::Angle subAngle = 2.0 / numSegments;
    psyqo::Angle currAngle = 0.0;
    for (int i = 0; i < (int)numSegments; ++i) {
        drawLineLocalSpace(
            {circle.radius * trig.sin(currAngle), 0.0, circle.radius * trig.cos(currAngle)},
            {circle.radius * trig.sin(currAngle + subAngle),
             0.0,
             circle.radius * trig.cos(currAngle + subAngle)},
            c);
        currAngle += subAngle;
    }
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
    psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(h);
}

void Renderer::drawArmature(const AnimatedModelObject& object, const Camera& camera)
{
    calculateViewModelMatrix(object, camera, true);

    const auto& armature = object.model.armature;
    if (armature.joints.empty()) {
        return;
    }
    const auto& rootJoint = armature.getRootJoint();
    drawArmature(armature, object, rootJoint, rootJoint.firstChild);
}

void Renderer::drawArmature(
    const Armature& armature,
    const AnimatedModelObject& object,
    const Joint& joint,
    Joint::JointId childId)
{
    using namespace psyqo::GTE;
    using namespace psyqo::GTE::Math;
    static constexpr auto L = PseudoRegister::Light;
    static constexpr auto V0 = PseudoRegister::V0;

    const auto& jt = object.jointGlobalTransforms[joint.id];

    static const auto boneStartL = psyqo::Vec3{};
    const auto boneEndL = (childId == Joint::NULL_JOINT_ID) ?
                              psyqo::Vec3{0.0, 0.1 / 8.0, 0.0} : // leaf bones have 0.1m length
                              armature.joints[childId].localTransform.translation;

    writeSafe<PseudoRegister::Light>(jt.rotation);

    const auto boneStartM = jt.transformPointMatrixLoaded<L, V0>(boneStartL);
    const auto boneEndM = jt.transformPointMatrixLoaded<L, V0>(boneEndL);

    const auto jointColor = psyqo::Color{.r = 255, .g = 255, .b = 128};
    drawLineLocalSpace(boneStartM, boneEndM, jointColor);

    auto currentJointId = joint.firstChild;
    while (currentJointId != Joint::NULL_JOINT_ID) {
        auto& child = armature.joints[currentJointId];
        drawArmature(armature, object, child, child.firstChild);
        currentJointId = child.nextSibling;
    }
}

TextureInfo Renderer::uploadTIM(const TimFile& tim)
{
    psyqo::Rect region =
        {.pos = {{.x = (std::int16_t)tim.pixDX, .y = (std::int16_t)tim.pixDY}},
         .size = {{.w = (std::int16_t)tim.pixW, .h = (std::int16_t)tim.pixH}}};
    gpu.uploadToVRAM((uint16_t*)tim.pixelsIdx.data(), region);

    // upload CLUT(s)
    // FIXME: support multiple cluts
    region =
        {.pos = {{.x = (std::int16_t)tim.clutDX, .y = (std::int16_t)tim.clutDY}},
         .size = {{.w = (std::int16_t)tim.clutW, .h = (std::int16_t)tim.clutH}}};
    gpu.uploadToVRAM(tim.cluts[0].colors.data(), region);

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
