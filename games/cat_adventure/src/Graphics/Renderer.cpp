#include "Renderer.h"

#include <common/syscalls/syscalls.h>
#include <psyqo/primitives/lines.hh>
#include <psyqo/primitives/rectangles.hh>
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

psyqo::Color interpColorImm(psyqo::Color input, uint32_t p)
{
    psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(p);
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(input.packed);
    psyqo::GTE::Kernels::dpcs();
    return {.packed = psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2>()};
}

psyqo::Color interpColorImmBack(psyqo::Color input, uint32_t p)
{
    // psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Safe>(eastl::max(4096 - (int)p, 0));
    psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(4096 - (int)p);
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
    // setFOV(350);

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
    static constexpr auto cullDistance = psyqo::FixedPoint<>(10.f);
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

    minAvgZ = 0xFFFFF;
    minSX = 500;
    minSY = 500;
    maxSX = -500;
    maxSY = -500;

    auto& model = object.model;
    const auto& armature = model.armature;

    if (armature.joints.empty()) {
        drawModelObject(object, camera);
        return;
    }

    for (auto& mesh : model.meshes) {
        drawMeshArmature(object, camera, armature, mesh);
    }

    { // fog fade rect
        auto& ot = getOrderingTable();
        auto& primBuffer = getPrimBuffer();

        auto& quadFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
        auto& q = quadFrag.primitive;

        // expand it just to be safe
        minSX -= 20;
        minSY -= 10;
        maxSX += 20;
        maxSY += 10;

        // we want it to be drawn after every primitive
        // inserting at minAvgZ means that some prims could possibly be rendered after it,
        // which is wrong
        minAvgZ -= 1;

        q.pointA.x = minSX;
        q.pointA.y = minSY;
        q.pointB.x = maxSX;
        q.pointB.y = minSY;
        q.pointC.x = minSX;
        q.pointC.y = maxSY;
        q.pointD.x = maxSX;
        q.pointD.y = maxSY;

        const auto color = interpColorImmBack(fogColor, minAvgP);
        q.setColorA(color);
        q.colorB = color;
        q.colorC = color;
        q.colorD = color;
        q.setSemiTrans();

        { // 4. restore the mask bit for other prims
            auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
                psyqo::Prim::MaskControl::Set::FromSource, psyqo::Prim::MaskControl::Test::No);
            ot.insert(maskBit, minAvgZ);
        }

        // 3. draw the quad itself
        ot.insert(quadFrag, minAvgZ);

        { // 2. Only draw to pixels which are with STP == 0
          // (animated objects must have textures with STP == 0 on all pixels)
            auto& maskBit = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
                psyqo::Prim::MaskControl::Set::ForceSet, psyqo::Prim::MaskControl::Test::Yes);
            ot.insert(maskBit, minAvgZ);
        }

        { // 1. previous prims might have changed the blending mode
            auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
            tpage.primitive.attr.setDithering(true).set(
                psyqo::Prim::TPageAttr::SemiTrans::FullBackAndFullFront);
            ot.insert(tpage, minAvgZ);
        }
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
    drawMesh2(object.mesh);
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

    wrk1.ocol[0] = prim.getColorA();
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
    static constexpr auto whiteTextureColor = psyqo::Color{.r = 128, .g = 128, .b = 128};

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

    uint32_t pa = 0;
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
            pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            tri2d.setColorA(interpColorImm(prim.getColorA()));

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

        if ((uint32_t)avgZ < minAvgZ) {
            minAvgZ = (uint32_t)avgZ;
            minAvgP = pa;
        }

        minSX = eastl::min({minSX, tri2d.pointA.x, tri2d.pointB.x, tri2d.pointC.x});
        maxSX = eastl::max({maxSX, tri2d.pointA.x, tri2d.pointB.x, tri2d.pointC.x});
        minSX = eastl::min({minSX, tri2d.pointA.y, tri2d.pointB.y, tri2d.pointC.y});
        maxSY = eastl::max({maxSY, tri2d.pointA.y, tri2d.pointB.y, tri2d.pointC.y});
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
            pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            quad2d.setColorA(interpColorImm(prim.getColorA()));

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

            wrk1.ocol[0] = quad2d.getColorA();
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

        if ((uint32_t)avgZ < minAvgZ) {
            minAvgZ = (uint32_t)avgZ;
            minAvgP = pa;
        }
        minSX =
            eastl::min({minSX, quad2d.pointA.x, quad2d.pointB.x, quad2d.pointC.x, quad2d.pointD.x});
        maxSX =
            eastl::max({maxSX, quad2d.pointA.x, quad2d.pointB.x, quad2d.pointC.x, quad2d.pointD.x});
        minSY =
            eastl::min({minSX, quad2d.pointA.y, quad2d.pointB.y, quad2d.pointC.y, quad2d.pointD.y});
        maxSY =
            eastl::max({maxSY, quad2d.pointA.y, quad2d.pointB.y, quad2d.pointC.y, quad2d.pointD.y});
    }
}

void Renderer::drawMesh2(const Mesh& mesh)
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

    static constexpr auto white = psyqo::Color{.r = 128, .g = 128, .b = 128};

    for (std::size_t i = 0; i < gt3s.size(); ++i) {
        auto& triFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudTriangle>();
        auto& tri2d = triFragFog.primitive;

        const auto& prim = gt3s[i];

        const auto& v0 = verts[gt3Offset + i * 3 + 0];
        const auto& v1 = verts[gt3Offset + i * 3 + 1];
        const auto& v2 = verts[gt3Offset + i * 3 + 2];

        uint32_t pa, pb, pc;
        if (fogEnabled) {
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
            psyqo::GTE::Kernels::rtps();

            pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            tri2d.setColorA(interpColorImmBack(fogColor, pa));

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
            psyqo::GTE::Kernels::rtps();

            pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            tri2d.colorB = interpColorImmBack(fogColor, pb);

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
            psyqo::GTE::Kernels::rtps();

            pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            tri2d.colorC = interpColorImmBack(fogColor, pc);
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
            uint32_t uvCPacked = reinterpret_cast<const std::uint32_t&>(prim.uvC);
            const auto addBias = static_cast<std::int16_t>((uvCPacked & 0xFFFF0000) >> 16);
            avgZ += addBias;
        }

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

        // tri2d = prim; // copy
        auto& triFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
        auto& triT = triFragT.primitive;

        /* quad2d.command = prim.command;
        quad2d.tpage = prim.tpage;
        quad2d.clutIndex = prim.clutIndex;
        quad2d.uvA = prim.uvA;
        quad2d.uvB = prim.uvB;
        quad2d.uvC = prim.uvC;
        quad2d.uvD = prim.uvD; */
        triT = prim; // copy

        triT.pointA = tri2d.pointA;
        triT.pointB = tri2d.pointB;
        triT.pointC = tri2d.pointC;

        triT.setColorA(interpColorImm(white, pa));
        triT.colorB = interpColorImm(white, pb);
        triT.colorC = interpColorImm(white, pc);

        triT.setSemiTrans();

        ot.insert(triFragT, avgZ);
        ot.insert(triFragFog, avgZ);
    }

    for (std::size_t i = 0; i < gt4s.size(); ++i) {
        const auto& v0 = verts[gt4Offset + i * 4 + 0];
        const auto& v1 = verts[gt4Offset + i * 4 + 1];
        const auto& v2 = verts[gt4Offset + i * 4 + 2];
        const auto& v3 = verts[gt4Offset + i * 4 + 3];

        auto& quadFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
        auto& quad2d = quadFragFog.primitive;
        quad2d.setOpaque();

        auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        auto& quadT = quadFragT.primitive;
        quadT.setSemiTrans();

        const auto& prim = gt4s[i];
        quadT.tpage = prim.tpage;
        quadT.clutIndex = prim.clutIndex;
        quadT.uvA = prim.uvA;
        quadT.uvB = prim.uvB;
        quadT.uvC = prim.uvC;
        quadT.uvD = prim.uvD;

        // load additional bias stored in padding
        uint32_t uvCPacked = reinterpret_cast<const std::uint32_t&>(prim.uvC);
        const auto addBias = static_cast<std::int16_t>((uvCPacked & 0xFFFF0000) >> 16);

        if (addBias == 500) {
            continue;
        }

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::Kernels::rtps();
        auto mac0 = psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>();
        auto sz0 = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ0>();

        quadT.setColorA(interpColorImm(white));

        uint32_t pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        quad2d.setColorA(interpColorImmBack(fogColor, pa));

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
        psyqo::GTE::Kernels::rtps();
        quadT.colorB = interpColorImm(white);

        uint32_t pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        quad2d.colorB = interpColorImmBack(fogColor, pb);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
        psyqo::GTE::Kernels::rtps();
        quadT.colorC = interpColorImm(white);

        uint32_t pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        quad2d.colorC = interpColorImmBack(fogColor, pc);

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            if (addBias != 2 && addBias != 3) {
                continue;
            }
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();
        quadT.colorD = interpColorImm(white);

        uint32_t pd = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        quad2d.colorD = interpColorImmBack(fogColor, pd);

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += bias + addBias; // add bias

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

        quadT.pointA = quad2d.pointA;
        quadT.pointB = quad2d.pointB;
        quadT.pointC = quad2d.pointC;
        quadT.pointD = quad2d.pointD;

        if (addBias == 3) { // textures with alpha
            quad2d.setSemiTrans();
            quadT.setOpaque();

            auto testcolor = psyqo::Color{.r = 52, .g = 48, .b = 56};
            auto c = quad2d.getColorA();
            if (testcolor.r == c.r && testcolor.g == c.g && testcolor.b == c.b) {
                /* ramsyscall_printf(
                    "mac0: %d, sz0: %d, calc(sz0): %d, otz: %d, WHAT: %d\n",
                    mac0,
                    sz0,
                    calcInterpFactor(sz0),
                    avgZ,
                    (int)pa); */
            }

            auto& maskBit2 = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
                psyqo::Prim::MaskControl::Set::FromSource, psyqo::Prim::MaskControl::Test::No);
            ot.insert(maskBit2, avgZ);

            ot.insert(quadFragFog, avgZ);

            auto& maskBit1 = primBuffer.allocateFragment<psyqo::Prim::MaskControl>(
                psyqo::Prim::MaskControl::Set::ForceSet, psyqo::Prim::MaskControl::Test::Yes);
            ot.insert(maskBit1, avgZ);

            ot.insert(quadFragT, avgZ);

        } else {
            ot.insert(quadFragT, avgZ);
            ot.insert(quadFragFog, avgZ);
        }
    }
}

void Renderer::drawTileQuad(
    int x,
    int z,
    int tileId,
    const ModelData& prefabs,
    const Camera& camera)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    static constexpr auto white = psyqo::Color{.r = 128, .g = 128, .b = 128};

    constexpr psyqo::FixedPoint tileScale = 0.125; // 1 / TILE_SIZE
    const auto tileHeight =
        (tileId != 1 && tileId != 2) ? psyqo::FixedPoint<>(0.0) : psyqo::FixedPoint<>(-0.02);

    if (tileId == 5 || tileId == 6) { // "model" tiles
        // mostly like drawMesh2, but moves each vertex into camera space without
        // using rotation/translation registers
        // - FIXME: remove copy-paste
        const auto& meshData = prefabs.meshes[tileId == 5 ? 4 : 5];
        const auto& g3s = meshData.g3;
        const auto& g4s = meshData.g4;
        const auto& gt3s = meshData.gt3;
        const auto& gt4s = meshData.gt4;
        const auto& verts = meshData.vertices;

        const auto g4Offset = g3s.size() * 3;
        const auto gt3Offset = g4Offset + g4s.size() * 4;
        const auto gt4Offset = gt3Offset + gt3s.size() * 3;

        const auto tileHeight = psyqo::FixedPoint<>(0.0);

        const auto originX =
            psyqo::GTE::Short(psyqo::FixedPoint(x, 0) * tileScale - camera.position.x);
        const auto originY = psyqo::GTE::Short(tileHeight - camera.position.y);
        const auto originZ =
            psyqo::GTE::Short(psyqo::FixedPoint(z, 0) * tileScale - camera.position.z);

        // FIXME: draw gt3s too!

        for (std::size_t i = 0; i < gt4s.size(); ++i) {
            auto v0 = verts[gt4Offset + i * 4 + 0];
            v0.pos.x += originX;
            v0.pos.y += originY;
            v0.pos.z += originZ;

            auto v1 = verts[gt4Offset + i * 4 + 1];
            v1.pos.x += originX;
            v1.pos.y += originY;
            v1.pos.z += originZ;

            auto v2 = verts[gt4Offset + i * 4 + 2];
            v2.pos.x += originX;
            v2.pos.y += originY;
            v2.pos.z += originZ;

            auto v3 = verts[gt4Offset + i * 4 + 3];
            v3.pos.x += originX;
            v3.pos.y += originY;
            v3.pos.z += originZ;

            const auto& prim = gt4s[i];

            auto& quadFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
            auto& quadFog = quadFragFog.primitive;
            quadFog.setOpaque();

            auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
            auto& quadT = quadFragT.primitive;
            quadT.setSemiTrans();

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
            psyqo::GTE::Kernels::rtps();

            quadT.setColorA(interpColorImm(white));

            uint32_t pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            quadFog.setColorA(interpColorImmBack(fogColor, pa));

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
            psyqo::GTE::Kernels::rtps();

            quadT.setColorB(interpColorImm(white));

            uint32_t pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            quadFog.setColorB(interpColorImmBack(fogColor, pb));

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
            psyqo::GTE::Kernels::rtps();

            quadT.setColorC(interpColorImm(white));

            uint32_t pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            quadFog.setColorC(interpColorImmBack(fogColor, pc));

            psyqo::GTE::Kernels::nclip();
            const auto dot =
                (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
            if (dot < 0) {
                continue;
            }

            psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadFog.pointA.packed);

            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
            psyqo::GTE::Kernels::rtps();

            quadT.setColorD(interpColorImm(white));

            uint32_t pd = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
            quadFog.setColorD(interpColorImmBack(fogColor, pd));

            psyqo::GTE::Kernels::avsz4();

            auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
            if (avgZ == 0) { // cull
                continue;
            }

            avgZ += 500;

            psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadFog.pointB.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadFog.pointC.packed);
            psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadFog.pointD.packed);

            quadT.pointA = quadFog.pointA;
            quadT.pointB = quadFog.pointB;
            quadT.pointC = quadFog.pointC;
            quadT.pointD = quadFog.pointD;

            quadT.tpage = prim.tpage;
            quadT.clutIndex = prim.clutIndex;
            quadT.uvA = prim.uvA;
            quadT.uvB = prim.uvB;
            quadT.uvC = prim.uvC;
            quadT.uvD = prim.uvD;

            ot.insert(quadFragT, avgZ);
            ot.insert(quadFragFog, avgZ);
        }

        return;
    }

    const auto v0 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(psyqo::FixedPoint(x, 0) * tileScale - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(psyqo::FixedPoint(z, 0) * tileScale - camera.position.z),
            },
    };

    const auto v1 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(psyqo::FixedPoint(x + 1, 0) * tileScale - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(psyqo::FixedPoint(z, 0) * tileScale - camera.position.z)},
    };

    const auto v2 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(psyqo::FixedPoint(x, 0) * tileScale - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(psyqo::FixedPoint(z + 1, 0) * tileScale - camera.position.z),
            },
    };

    const auto v3 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(psyqo::FixedPoint(x + 1, 0) * tileScale - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(psyqo::FixedPoint(z + 1, 0) * tileScale - camera.position.z),
            },
    };

    auto& quadFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
    auto& quadFog = quadFragFog.primitive;
    quadFog.setOpaque();

    auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
    auto& quadT = quadFragT.primitive;
    quadT.setSemiTrans();

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
    psyqo::GTE::Kernels::rtps();

    quadT.setColorA(interpColorImm(white));

    uint32_t pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorA(interpColorImmBack(fogColor, pa));

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
    psyqo::GTE::Kernels::rtps();

    quadT.setColorB(interpColorImm(white));

    uint32_t pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorB(interpColorImmBack(fogColor, pb));

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
    psyqo::GTE::Kernels::rtps();

    quadT.setColorC(interpColorImm(white));

    uint32_t pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorC(interpColorImmBack(fogColor, pc));

    psyqo::GTE::Kernels::nclip();
    const auto dot = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
    if (dot < 0) {
        return;
    }

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadFog.pointA.packed);

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
    psyqo::GTE::Kernels::rtps();

    quadT.setColorD(interpColorImm(white));

    uint32_t pd = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorD(interpColorImmBack(fogColor, pd));

    psyqo::GTE::Kernels::avsz4();

    auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
    if (avgZ == 0) { // cull
        return;
    }

    avgZ += 500;

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadFog.pointB.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadFog.pointC.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadFog.pointD.packed);

    quadT.tpage.setPageX(5)
        .setPageY(0)
        .set(psyqo::Prim::TPageAttr::ColorMode::Tex8Bits)
        .set(psyqo::Prim::TPageAttr::SemiTrans::FullBackAndFullFront);

#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))
    quadT.clutIndex = psyqo::PrimPieces::ClutIndex(0, 240);

    if (tileId == 0) {
        quadT.uvA.u = 0;
        quadT.uvA.v = 128;
        quadT.uvB.u = 127;
        quadT.uvB.v = 128;
        quadT.uvC.u = 0;
        quadT.uvC.v = 255;
        quadT.uvD.u = 127;
        quadT.uvD.v = 255;
    } else if (tileId == 1) {
        quadT.uvA.u = 64;
        quadT.uvA.v = 0;
        quadT.uvB.u = 127;
        quadT.uvB.v = 0;
        quadT.uvC.u = 64;
        quadT.uvC.v = 63;
        quadT.uvD.u = 127;
        quadT.uvD.v = 63;
    } else if (tileId == 2) {
        quadT.uvA.u = 0;
        quadT.uvA.v = 0;
        quadT.uvB.u = 63;
        quadT.uvB.v = 0;
        quadT.uvC.u = 0;
        quadT.uvC.v = 63;
        quadT.uvD.u = 63;
        quadT.uvD.v = 63;
    } else if (tileId == 3) {
        quadT.uvA.u = 0;
        quadT.uvA.v = 96;
        quadT.uvB.u = 31;
        quadT.uvB.v = 96;
        quadT.uvC.u = 0;
        quadT.uvC.v = 127;
        quadT.uvD.u = 31;
        quadT.uvD.v = 127;
    } else {
        quadT.uvA.u = 0;
        quadT.uvA.v = 80;
        quadT.uvB.u = 31;
        quadT.uvB.v = 80;
        quadT.uvC.u = 0;
        quadT.uvC.v = 95;
        quadT.uvD.u = 31;
        quadT.uvD.v = 95;
    }

    quadT.pointA = quadFog.pointA;
    quadT.pointB = quadFog.pointB;
    quadT.pointC = quadFog.pointC;
    quadT.pointD = quadFog.pointD;

    ot.insert(quadFragT, avgZ);
    ot.insert(quadFragFog, avgZ);
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

void Renderer::drawPointWorldSpace(
    const Camera& camera,
    const psyqo::Vec3& p,
    const psyqo::Color& c,
    bool cameraViewLoaded)
{
    auto& primBuffer = getPrimBuffer();

    if (!cameraViewLoaded) {
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camera.view.rotation);
    }

    auto posCamSpace = p - camera.position;
    // R * V0
    psyqo::GTE::Math::matrixVecMul3<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(posCamSpace, &posCamSpace);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(psyqo::Vec3{});
    psyqo::GTE::Kernels::rtps();

    auto& pixelFrag = primBuffer.allocateFragment<psyqo::Prim::Pixel>();
    auto& pixel = pixelFrag.primitive;
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&pixel.position.packed);
    pixel.setColor(c);

    gpu.chain(pixelFrag);
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

    auto div = (h * 0x20000) / sz;
    if (div > 0x1FFFF) {
        div = 0x1FFFF;
    }
    const auto mac0 = (div + 1) / 2 * dqa + dqb;
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
