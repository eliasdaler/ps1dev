#include "Renderer.h"

#include <common/syscalls/syscalls.h>
#include <psyqo/primitives/lines.hh>
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/soft-math.hh>

#include <Camera.h>
#include <Common.h>
#include <Graphics/TimFile.h>
#include <Math/Math.h>
#include <Math/gte-math.h>
#include <Object.h>
#include <TileMap.h>

#include <cstring> // memcpy

#include "Subdivision.h"

static constexpr auto textureNeutral = psyqo::Color{.r = 128, .g = 128, .b = 128};
static constexpr auto floorBias = 500;

namespace
{
template<psyqo::Primitive T>
int16_t getAddBias(const T& prim)
{
    return prim.uvC.user;
}

static constexpr psyqo::FixedPoint<> toWorldCoords(std::uint16_t idx)
{
    // only for Tile::SIZE == 8!!!
    // 12 - 3 == 9 (same as doing psyqo::FixedPoint<>(idx, 0) * tileScale
    return psyqo::FixedPoint<>(idx << 9, psyqo::FixedPoint<>::RAW);
}

// Interpolate color immediately after doing rtps (IR0 has "p" in it already)
psyqo::Color interpColorImm(psyqo::Color input)
{
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(input.packed);
    psyqo::GTE::Kernels::dpcs();
    return {.packed = psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2>()};
}

// Interpolate from input to fc
// (0 = input, 1 = fc)
psyqo::Color interpColor(psyqo::Color input, uint32_t p)
{
    psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(p);
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(input.packed);
    psyqo::GTE::Kernels::dpcs();
    return {.packed = psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2>()};
}

// Interpolate from input to fc, but instead of normal dpcs
// (0 = input, 1 = fc), do (0 = fc, 1 = input)
psyqo::Color interpColorBack(psyqo::Color input, uint32_t p)
{
    psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(4096 - (int)p);
    psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(input.packed);
    psyqo::GTE::Kernels::dpcs();
    return {.packed = psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2>()};
}

/* Adopted from https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/ */
int orient2d(int ax, int ay, int bx, int by, int cx, int cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

void rasterizeTriangle(eastl::bitset<Renderer::MAX_TILES_DIM * Renderer::MAX_TILES_DIM>& tileSeen,
    int x1,
    int y1,
    int x2,
    int y2,
    int x3,
    int y3)
{
    int minX = eastl::min({x1, x2, x3});
    int maxX = eastl::max({x1, x2, x3});
    int minY = eastl::min({y1, y2, y3});
    int maxY = eastl::max({y1, y2, y3});

    minX = eastl::max(0, minX);
    maxX = eastl::min(Renderer::MAX_TILES_DIM - 1, maxX);
    minY = eastl::max(0, minY);
    maxY = eastl::min(Renderer::MAX_TILES_DIM - 1, maxY);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            int w0 = orient2d(x2, y2, x3, y3, x, y);
            int w1 = orient2d(x3, y3, x1, y1, x, y);
            int w2 = orient2d(x1, y1, x2, y2, x, y);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                tileSeen[y * Renderer::MAX_TILES_DIM + x] = 1;
            }
        }
    }
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
        psyqo::GTE::Math::multiplyMatrix33<psyqo::GTE::PseudoRegister::Rotation,
            psyqo::GTE::PseudoRegister::V0>(
            camera.view.rotation, object.transform.rotation, &viewModelMatrix);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(viewModelMatrix);
    }

    // what 4th column would be if we did V * M
    auto posCamSpace = object.transform.translation - camera.position;
    if (!setViewRot) {
        psyqo::GTE::Math::matrixVecMul3<psyqo::GTE::PseudoRegister::Rotation,
            psyqo::GTE::PseudoRegister::V0>(posCamSpace, &posCamSpace);
    } else {
        // TODO: use L?
        psyqo::SoftMath::matrixVecMul3(camera.view.rotation, posCamSpace, &posCamSpace);
    }
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(posCamSpace);
}

bool Renderer::shouldCullObject(const Object& object, const Camera& camera) const
{
    static constexpr auto cullDistance = psyqo::FixedPoint<>(6.f);
    return math::distanceSq(object.getPosition(), camera.position) > cullDistance;
}

void Renderer::drawModelObject(ModelObject& object, const Camera& camera, bool setViewRot)
{
    if (shouldCullObject(object, camera)) {
        return;
    }
    calculateViewModelMatrix(object, camera, setViewRot);

    drawModel(object.model);
}

void Renderer::drawAnimatedModelObject(AnimatedModelObject& object,
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

        const auto color = interpColorBack(fogColor, minAvgP);
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

void Renderer::drawMeshArmature(const AnimatedModelObject& object,
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
        multiplyMatrix33<PseudoRegister::Rotation, PseudoRegister::V0>(
            camera.view.rotation, t1.rotation, &t2.rotation);

        // Instead of using camera.view.translation (which is V * (-camPos)),
        // we're going into the camera/view space and do calculations there
        t2.translation = t1.translation - camera.position;
        matrixVecMul3<PseudoRegister::Rotation, // camera.view.rotation
            PseudoRegister::V0>(t2.translation, &t2.translation);

        writeUnsafe<PseudoRegister::Rotation>(t2.rotation);
        writeSafe<PseudoRegister::Translation>(t2.translation);
    }

    if (fogEnabled) {
        drawMeshFog(meshData);
    } else {
        drawMesh(meshData);
    }
}

void Renderer::drawMeshObject(MeshObject& object, const Camera& camera, bool setViewRot)
{
    if (shouldCullObject(object, camera)) {
        return;
    }
    calculateViewModelMatrix(object, camera, setViewRot);
    if (fogEnabled) {
        drawMeshStaticFog(object.mesh);
    } else {
        drawMeshStatic(object.mesh);
    }
}

void Renderer::drawModel(const Model& model)
{
    for (const auto& mesh : model.meshes) {
        if (fogEnabled) {
            drawMeshFog(*mesh.meshData);
        } else {
            drawMesh(*mesh.meshData);
        }
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

void Renderer::drawMeshFog(const MeshData& meshData)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    const auto& g3s = meshData.g3;
    const auto& g4s = meshData.g4;
    const auto& gt3s = meshData.gt3;
    const auto& gt4s = meshData.gt4;

    const auto g4Offset = g3s.size() * 3;
    const auto gt3Offset = g4Offset + g4s.size() * 4;
    const auto gt4Offset = gt3Offset + gt3s.size() * 3;

    const auto& verts = meshData.vertices;

    uint32_t pa = 0;
    const auto gt3ss = gt3s.size();
    for (std::size_t i = 0; i < gt3ss; ++i) {
        auto& triFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
        auto& triT = triFrag.primitive;

        const auto& v0 = verts[gt3Offset + i * 3 + 0];
        const auto& v1 = verts[gt3Offset + i * 3 + 1];
        const auto& v2 = verts[gt3Offset + i * 3 + 2];

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::Kernels::rtps();

        // do while rtps
        const auto& prim = gt3s[i];
        triT.tpage = prim.tpage;

        pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        triT.setColorA(interpColorImm(prim.getColorA()));

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
        psyqo::GTE::Kernels::rtps();

        // do while rtps
        triT.clutIndex = prim.clutIndex;
        triT.uvA = prim.uvA;

        const auto p1 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        triT.colorB = interpColorImm(prim.colorB);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
        psyqo::GTE::Kernels::rtps();

        // do while rtps
        triT.uvB = prim.uvB;
        triT.uvC = prim.uvC;

        const auto p2 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        triT.colorC = interpColorImm(prim.colorC);

        psyqo::GTE::Kernels::nclip();
        const auto addBias = getAddBias(prim);

        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            continue;
        }

        psyqo::GTE::Kernels::avsz3();

        auto avgZ = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            if (addBias != 2) {
                continue;
            }
        }

        avgZ += bias + addBias; // add bias

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&triT.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&triT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&triT.pointC.packed);

        ot.insert(triFrag, avgZ);

        if ((uint32_t)avgZ < minAvgZ) {
            minAvgZ = (uint32_t)avgZ;
            minAvgP = pa;
        }

        minSX = eastl::min({minSX, triT.pointA.x, triT.pointB.x, triT.pointC.x});
        maxSX = eastl::max({maxSX, triT.pointA.x, triT.pointB.x, triT.pointC.x});
        minSY = eastl::min({minSY, triT.pointA.y, triT.pointB.y, triT.pointC.y});
        maxSY = eastl::max({maxSY, triT.pointA.y, triT.pointB.y, triT.pointC.y});
    }

    const auto gt4ss = gt4s.size();
    for (std::size_t i = 0; i < gt4ss; ++i) {
        const auto& v0 = verts[gt4Offset + i * 4 + 0];
        const auto& v1 = verts[gt4Offset + i * 4 + 1];
        const auto& v2 = verts[gt4Offset + i * 4 + 2];
        const auto& v3 = verts[gt4Offset + i * 4 + 3];

        auto& quadFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        auto& quadT = quadFrag.primitive;

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::Kernels::rtps();

        // do while rtps
        const auto& prim = gt4s[i];
        quadT.tpage = prim.tpage;

        pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
        quadT.setColorA(interpColorImm(prim.getColorA()));

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
        psyqo::GTE::Kernels::rtps();

        // do while rtps
        quadT.clutIndex = prim.clutIndex;
        quadT.uvA = prim.uvA;
        quadT.uvB = prim.uvB;

        quadT.colorB = interpColorImm(prim.colorB);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
        psyqo::GTE::Kernels::rtps();

        // do while rtps
        quadT.uvC = prim.uvC;
        quadT.uvD = prim.uvD;

        quadT.colorC = interpColorImm(prim.colorC);

        psyqo::GTE::Kernels::nclip();
        const auto addBias = getAddBias(prim);

        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            if (addBias != 2) {
                continue;
            }
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        quadT.colorD = interpColorImm(prim.colorD);

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += bias + addBias; // add bias

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadT.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadT.pointD.packed);

        ot.insert(quadFrag, avgZ);

        if ((uint32_t)avgZ < minAvgZ) {
            minAvgZ = (uint32_t)avgZ;
            minAvgP = pa;
        }

        minSX = eastl::min({minSX, quadT.pointA.x, quadT.pointB.x, quadT.pointC.x, quadT.pointD.x});
        maxSX = eastl::max({maxSX, quadT.pointA.x, quadT.pointB.x, quadT.pointC.x, quadT.pointD.x});
        minSY = eastl::min({minSY, quadT.pointA.y, quadT.pointB.y, quadT.pointC.y, quadT.pointD.y});
        maxSY = eastl::max({maxSY, quadT.pointA.y, quadT.pointB.y, quadT.pointC.y, quadT.pointD.y});
    }
}

void Renderer::drawMesh(const MeshData& meshData)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    const auto& g3s = meshData.g3;
    const auto& g4s = meshData.g4;
    const auto& gt3s = meshData.gt3;
    const auto& gt4s = meshData.gt4;

    const auto g4Offset = g3s.size() * 3;
    const auto gt3Offset = g4Offset + g4s.size() * 4;
    const auto gt4Offset = gt3Offset + gt3s.size() * 3;

    const auto& verts = meshData.vertices;

    uint32_t pa = 0;
    for (std::size_t i = 0; i < gt3s.size(); ++i) {
        auto& triFrag = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
        auto& tri2d = triFrag.primitive;

        const auto& prim = gt3s[i];
        tri2d = prim; // copy

        const auto& v0 = verts[gt3Offset + i * 3 + 0];
        const auto& v1 = verts[gt3Offset + i * 3 + 1];
        const auto& v2 = verts[gt3Offset + i * 3 + 2];

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
        { // load additional bias stored in padding
            const auto addBias = getAddBias(prim);
            avgZ += addBias;
        }

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&tri2d.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&tri2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&tri2d.pointC.packed);

        tri2d.setColorA(prim.getColorA());
        tri2d.setColorB(prim.getColorB());
        tri2d.setColorC(prim.getColorC());

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

        // quad2d = prim; // copy
        quad2d.tpage = prim.tpage;
        quad2d.clutIndex = prim.clutIndex;
        quad2d.uvA = prim.uvA;
        quad2d.uvB = prim.uvB;
        quad2d.uvC = prim.uvC;
        quad2d.uvD = prim.uvD;

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
        psyqo::GTE::Kernels::rtpt();

        // load additional bias stored in padding
        const auto addBias = getAddBias(prim);

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

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += bias + addBias; // add bias

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        quad2d.setColorA(prim.getColorA());
        quad2d.setColorB(prim.getColorB());
        quad2d.setColorC(prim.getColorC());
        quad2d.setColorD(prim.getColorD());

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quad2d.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quad2d.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quad2d.pointD.packed);

        ot.insert(quadFrag, avgZ);
    }
}

void Renderer::drawMeshStaticFog(const Mesh& mesh)
{
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

    const auto gt3ss = gt3s.size();
    for (std::size_t i = 0; i < gt3ss; ++i) {
        auto& triFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
        auto& triT = triFragT.primitive;
        triT.setSemiTrans();

        const auto& v0 = verts[gt3Offset + i * 3 + 0];
        const auto& v1 = verts[gt3Offset + i * 3 + 1];
        const auto& v2 = verts[gt3Offset + i * 3 + 2];

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::Kernels::rtps();

        // have some time while rtps does stuff
        const auto& prim = gt3s[i];

        triT.setColorA(interpColorImm(textureNeutral));

        const auto pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
        psyqo::GTE::Kernels::rtps();

        // have some time while rtps does stuff
        triT.tpage = prim.tpage;
        triT.clutIndex = prim.clutIndex;

        triT.colorB = interpColorImm(textureNeutral);
        const auto pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
        psyqo::GTE::Kernels::rtps();

        // have some time while rtps does stuff
        triT.uvA = prim.uvA;
        triT.uvB = prim.uvB;
        triT.uvC = prim.uvC;

        triT.colorC = interpColorImm(textureNeutral);
        const auto pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&triT.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&triT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&triT.pointC.packed);

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
            const auto addBias = getAddBias(prim);
            avgZ += addBias;
        }

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        auto& triFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudTriangle>();
        auto& triFog = triFragFog.primitive;

        triFog.setColorA(interpColorBack(fogColor, pa));
        triFog.colorB = interpColorBack(fogColor, pb);
        triFog.colorC = interpColorBack(fogColor, pc);

        triFog.pointA = triT.pointA;
        triFog.pointB = triT.pointB;
        triFog.pointC = triT.pointC;

        ot.insert(triFragT, avgZ);
        ot.insert(triFragFog, avgZ);
    }

    const auto gt4ss = gt4s.size();
    for (std::size_t i = 0; i < gt4ss; ++i) {
        const auto& v0 = verts[gt4Offset + i * 4 + 0];
        const auto& v1 = verts[gt4Offset + i * 4 + 1];
        const auto& v2 = verts[gt4Offset + i * 4 + 2];
        const auto& v3 = verts[gt4Offset + i * 4 + 3];

        auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        auto& quadT = quadFragT.primitive;
        quadT.setSemiTrans();

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::Kernels::rtps();

        // do stuff while rtps works
        const auto& prim = gt4s[i];

        quadT.setColorA(interpColorImm(textureNeutral));
        uint32_t pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
        psyqo::GTE::Kernels::rtps();

        // do stuff while rtps works
        quadT.tpage = prim.tpage;
        quadT.clutIndex = prim.clutIndex;

        quadT.colorB = interpColorImm(textureNeutral);
        uint32_t pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
        psyqo::GTE::Kernels::rtps();

        // do stuff while rtps works
        quadT.uvA = prim.uvA;
        quadT.uvB = prim.uvB;

        quadT.colorC = interpColorImm(textureNeutral);
        uint32_t pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::Kernels::nclip();
        const auto addBias = getAddBias(prim);

        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            if (addBias != 2 && addBias != 3) {
                continue;
            }
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        // do stuff while rtps works
        quadT.uvC = prim.uvC;
        quadT.uvD = prim.uvD;

        quadT.colorD = interpColorImm(textureNeutral);
        uint32_t pd = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += bias + addBias; // add bias

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadT.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadT.pointD.packed);

        auto& quadFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
        auto& quadFog = quadFragFog.primitive;

        quadFog.setColorA(interpColorBack(fogColor, pa));
        quadFog.colorB = interpColorBack(fogColor, pb);
        quadFog.colorC = interpColorBack(fogColor, pc);
        quadFog.colorD = interpColorBack(fogColor, pd);

        quadFog.pointA = quadT.pointA;
        quadFog.pointB = quadT.pointB;
        quadFog.pointC = quadT.pointC;
        quadFog.pointD = quadT.pointD;

        if (addBias == 3) { // textures with alpha
            quadFog.setSemiTrans();
            quadT.setOpaque();

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

void Renderer::drawMeshStatic(const Mesh& mesh)
{
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

    for (std::size_t i = 0; i < gt3s.size(); ++i) {
        auto& triFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
        auto& triT = triFragT.primitive;

        const auto& prim = gt3s[i];
        triT.tpage = prim.tpage;
        triT.clutIndex = prim.clutIndex;
        triT.uvA = prim.uvA;
        triT.uvB = prim.uvB;
        triT.uvC = prim.uvC;

        const auto& v0 = verts[gt3Offset + i * 3 + 0];
        const auto& v1 = verts[gt3Offset + i * 3 + 1];
        const auto& v2 = verts[gt3Offset + i * 3 + 2];

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
        psyqo::GTE::Kernels::rtpt();

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&triT.pointA.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&triT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&triT.pointC.packed);

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
            const auto addBias = getAddBias(prim);
            avgZ += addBias;
        }

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        triT.setColorA(prim.getColorA());
        triT.setColorB(prim.getColorB());
        triT.setColorC(prim.getColorC());

        ot.insert(triFragT, avgZ);
    }

    for (std::size_t i = 0; i < gt4s.size(); ++i) {
        const auto& v0 = verts[gt4Offset + i * 4 + 0];
        const auto& v1 = verts[gt4Offset + i * 4 + 1];
        const auto& v2 = verts[gt4Offset + i * 4 + 2];
        const auto& v3 = verts[gt4Offset + i * 4 + 3];

        auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        auto& quadT = quadFragT.primitive;

        const auto& prim = gt4s[i];
        quadT.tpage = prim.tpage;
        quadT.clutIndex = prim.clutIndex;
        quadT.uvA = prim.uvA;
        quadT.uvB = prim.uvB;
        quadT.uvC = prim.uvC;
        quadT.uvD = prim.uvD;

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
        psyqo::GTE::Kernels::rtpt();

        // load additional bias stored in padding (while rtpt)
        const auto addBias = getAddBias(prim);

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            if (addBias != 2 && addBias != 3) {
                continue;
            }
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += bias + addBias; // add bias

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadT.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadT.pointD.packed);

        quadT.setColorA(prim.getColorA());
        quadT.setColorB(prim.getColorB());
        quadT.setColorC(prim.getColorC());
        quadT.setColorD(prim.getColorD());

        ot.insert(quadFragT, avgZ);
    }
}

void Renderer::calculateTileVisibility(const Camera& camera)
{
    const auto front = psyqo::Vec3{
        .x = trig.sin(camera.rotation.y),
        .y = 0.0,
        .z = trig.cos(camera.rotation.y),
    };

    const auto fov = psyqo::Angle(0.27);
    auto viewDistSide = psyqo::FixedPoint(3.5);

    const auto yaw = camera.rotation.y;
    psyqo::Vec3 origin = camera.position - front * 0.1;

    if (camera.rotation.x > 0.15) {
        // TODO: hande better - probably need to calculate based on pitch somehow
        origin = camera.position - front * 0.5;
        viewDistSide = psyqo::FixedPoint(4.0);
    }

    auto frustumLeft = origin + psyqo::Vec3{.x = viewDistSide * trig.sin(yaw - fov),
                                    .y = 0,
                                    .z = viewDistSide * trig.cos(yaw - fov)};
    auto frustumRight = origin + psyqo::Vec3{.x = viewDistSide * trig.sin(yaw + fov),
                                     .y = 0,
                                     .z = viewDistSide * trig.cos(yaw + fov)};

    const auto minX = eastl::min({origin.x, frustumLeft.x, frustumRight.x});
    const auto minZ = eastl::min({origin.z, frustumLeft.z, frustumRight.z});
    const auto maxX = eastl::max({origin.x, frustumLeft.x, frustumRight.x});
    const auto maxZ = eastl::max({origin.z, frustumLeft.z, frustumRight.z});

    minTileX = (minX * Tile::SIZE).floor();
    minTileZ = (minZ * Tile::SIZE).floor();
    maxTileX = (maxX * Tile::SIZE).ceil();
    maxTileZ = (maxZ * Tile::SIZE).ceil();

    const auto originTileX = (origin.x * Tile::SIZE).floor();
    const auto originTileZ = (origin.z * Tile::SIZE).floor();

    const auto pLeftTileX = (frustumLeft.x * Tile::SIZE).floor();
    const auto pLeftTiLeZ = (frustumLeft.z * Tile::SIZE).floor();

    const auto pRightTileX = (frustumRight.x * Tile::SIZE).floor();
    const auto pRightTileZ = (frustumRight.z * Tile::SIZE).floor();

    tileSeen.reset();

    rasterizeTriangle(tileSeen,
        maxTileZ - originTileZ,
        maxTileX - originTileX,
        maxTileZ - pLeftTiLeZ,
        maxTileX - pLeftTileX,
        maxTileZ - pRightTileZ,
        maxTileX - pRightTileX);
}

void Renderer::drawTiles(const ModelData& tileModels, const TileMap& tileMap, const Camera& camera)
{
    calculateTileVisibility(camera);

    numTilesDrawn = 0;

    // all translation is handled by us moving tiles into camera space manually
    static constexpr psyqo::Vec3 v{};
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Translation>(v);

    const auto fog = fogEnabled;
    for (int16_t z = minTileZ; z <= maxTileZ; ++z) {
        for (int16_t x = minTileX; x <= maxTileX; ++x) {
            const int xrel = maxTileX - x;
            const int zrel = maxTileZ - z;
            if (xrel >= 0 && xrel < MAX_TILES_DIM && zrel >= 0 && zrel < MAX_TILES_DIM &&
                tileSeen[xrel * MAX_TILES_DIM + zrel] == 1) {
                const auto tileIndex = TileIndex{x, z};
                const auto tile = tileMap.getTile(tileIndex);
                if (tile.tileId == Tile::NULL_TILE_ID) {
                    continue;
                }
                if (fog) {
                    drawTileFog(tileIndex, tile, tileMap.tileset, tileModels, camera);
                } else {
                    drawTile(tileIndex, tile, tileMap.tileset, tileModels, camera);
                }
                ++numTilesDrawn;
            }
        }
    }
}

void Renderer::drawTileFog(TileIndex tileIndex,
    const Tile& tile,
    const Tileset& tileset,
    const ModelData& tileMeshes,
    const Camera& camera)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    const auto tileId = tile.tileId;
    const auto& tileInfo = tileset.getTileInfo(tileId);
    const auto modelId = tileInfo.modelId;

    if (modelId != TileInfo::NULL_MODEL_ID) { // "model" tiles
        drawTileMeshFog(tileIndex, tile, tileset, tileMeshes.meshes[modelId], camera);
        return;
    }

    const int x = tileIndex.x;
    const int z = tileIndex.z;

    const auto tileHeight = psyqo::FixedPoint<>(tileInfo.height);

    const auto v0 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z) - camera.position.z),
            },
    };

    const auto v1 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x + 1) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z) - camera.position.z),
            },
    };

    const auto v2 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z + 1) - camera.position.z),
            },
    };

    const auto v3 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x + 1) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z + 1) - camera.position.z),
            },
    };

    auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
    auto& quadT = quadFragT.primitive;

    // TODO: set this for a given chunk/map?
    quadT.tpage.setPageX(5)
        .setPageY(0)
        .set(psyqo::Prim::TPageAttr::ColorMode::Tex8Bits)
        .set(psyqo::Prim::TPageAttr::SemiTrans::FullBackAndFullFront);
#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))
    quadT.clutIndex = psyqo::PrimPieces::ClutIndex(0, 240);

    auto& quadFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
    auto& quadFog = quadFragFog.primitive;

    quadFog.setOpaque();
    quadT.setSemiTrans();

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
    psyqo::GTE::Kernels::rtps();

    // do while rtps does work
    quadT.uvA.u = tileInfo.u0;
    quadT.uvA.v = tileInfo.v0;

    quadT.setColorA(interpColorImm(textureNeutral));
    uint32_t pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorA(interpColorBack(fogColor, pa));

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
    psyqo::GTE::Kernels::rtps();

    // do while rtps does work
    quadT.uvB.u = tileInfo.u1;
    quadT.uvB.v = tileInfo.v0;

    quadT.setColorB(interpColorImm(textureNeutral));
    uint32_t pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorB(interpColorBack(fogColor, pb));

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
    psyqo::GTE::Kernels::rtps();

    // do while rtps does work
    quadT.uvC.u = tileInfo.u0;
    quadT.uvC.v = tileInfo.v1;

    quadT.setColorC(interpColorImm(textureNeutral));
    uint32_t pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorC(interpColorBack(fogColor, pc));

    psyqo::GTE::Kernels::nclip();
    const auto dot = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
    if (dot < 0) {
        return;
    }

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointA.packed);

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
    psyqo::GTE::Kernels::rtps();

    // do while rtps does work
    quadT.uvD.u = tileInfo.u1;
    quadT.uvD.v = tileInfo.v1;

    quadT.setColorD(interpColorImm(textureNeutral));
    uint32_t pd = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
    quadFog.setColorD(interpColorBack(fogColor, pd));

    psyqo::GTE::Kernels::avsz4();

    auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
    if (avgZ == 0) { // cull
        return;
    }

    avgZ += floorBias;
    if (tileHeight < 0.0) { // TEMP HACK: this makes pavements less glitchy
        avgZ += 200;
    }

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointB.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadT.pointC.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadT.pointD.packed);

    quadFog.pointA = quadT.pointA;
    quadFog.pointB = quadT.pointB;
    quadFog.pointC = quadT.pointC;
    quadFog.pointD = quadT.pointD;

    ot.insert(quadFragT, avgZ);
    ot.insert(quadFragFog, avgZ);
}

void Renderer::drawTile(TileIndex tileIndex,
    const Tile& tile,
    const Tileset& tileset,
    const ModelData& tileMeshes,
    const Camera& camera)
{
    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    const auto tileId = tile.tileId;
    const auto& tileInfo = tileset.getTileInfo(tileId);
    const auto modelId = tileInfo.modelId;

    if (modelId != TileInfo::NULL_MODEL_ID) { // "model" tiles
        drawTileMesh(tileIndex, tile, tileset, tileMeshes.meshes[modelId], camera);
        return;
    }

    const int x = tileIndex.x;
    const int z = tileIndex.z;

    const auto tileHeight = psyqo::FixedPoint<>(tileInfo.height);

    const auto v0 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z) - camera.position.z),
            },
    };

    const auto v1 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x + 1) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z) - camera.position.z),
            },
    };

    const auto v2 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z + 1) - camera.position.z),
            },
    };

    const auto v3 = Vec3Pad{
        .pos =
            psyqo::GTE::PackedVec3{
                psyqo::GTE::Short(toWorldCoords(x + 1) - camera.position.x),
                psyqo::GTE::Short(tileHeight - camera.position.y),
                psyqo::GTE::Short(toWorldCoords(z + 1) - camera.position.z),
            },
    };

    auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
    auto& quadT = quadFragT.primitive;
    // TODO: set this for a given chunk/map?
    quadT.tpage.setPageX(5)
        .setPageY(0)
        .set(psyqo::Prim::TPageAttr::ColorMode::Tex8Bits)
        .set(psyqo::Prim::TPageAttr::SemiTrans::FullBackAndFullFront);
#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))
    quadT.clutIndex = psyqo::PrimPieces::ClutIndex(0, 240);

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
    psyqo::GTE::Kernels::rtpt();

    // do while rtpt
    quadT.uvA.u = tileInfo.u0;
    quadT.uvA.v = tileInfo.v0;
    quadT.uvB.u = tileInfo.u1;
    quadT.uvB.v = tileInfo.v0;
    quadT.uvC.u = tileInfo.u0;
    quadT.uvC.v = tileInfo.v1;
    quadT.uvD.u = tileInfo.u1;
    quadT.uvD.v = tileInfo.v1;

    psyqo::GTE::Kernels::nclip();
    const auto dot = (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
    if (dot < 0) {
        return;
    }

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointA.packed);

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
    psyqo::GTE::Kernels::rtps();

    // do while rtps is doing this
    quadT.setColorA(textureNeutral);
    quadT.setColorB(textureNeutral);
    quadT.setColorC(textureNeutral);
    quadT.setColorD(textureNeutral);

    psyqo::GTE::Kernels::avsz4();

    auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
    if (avgZ == 0) { // cull
        return;
    }

    avgZ += floorBias;

    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointB.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadT.pointC.packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadT.pointD.packed);

    ot.insert(quadFragT, avgZ);
}

void Renderer::drawTileMeshFog(TileIndex tileIndex,
    const Tile& tile,
    const Tileset& tileset,
    const MeshData& meshData,
    const Camera& camera)
{
    const int x = tileIndex.x;
    const int z = tileIndex.z;

    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    // mostly like drawMeshStatic, but moves each vertex into camera space without
    // using rotation/translation registers
    // - FIXME: remove copy-paste
    const auto& g3s = meshData.g3;
    const auto& g4s = meshData.g4;
    const auto& gt3s = meshData.gt3;
    const auto& gt4s = meshData.gt4;
    const auto& verts = meshData.vertices;

    const auto g4Offset = g3s.size() * 3;
    const auto gt3Offset = g4Offset + g4s.size() * 4;
    const auto gt4Offset = gt3Offset + gt3s.size() * 3;

    const auto& tileInfo = tileset.getTileInfo(tile.tileId);
    const auto tileHeight = psyqo::FixedPoint<>(tileInfo.height);

    const auto originX =
        psyqo::GTE::Short(psyqo::FixedPoint<>(x, 0) * Tile::SCALE - camera.position.x);
    const auto originY = psyqo::GTE::Short(tileHeight - camera.position.y);
    const auto originZ =
        psyqo::GTE::Short(psyqo::FixedPoint<>(z, 0) * Tile::SCALE - camera.position.z);

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

        auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        auto& quadT = quadFragT.primitive;
        quadT.setSemiTrans();

        quadT.tpage = prim.tpage;
        quadT.clutIndex = prim.clutIndex;
        quadT.uvA = prim.uvA;
        quadT.uvB = prim.uvB;
        quadT.uvC = prim.uvC;
        quadT.uvD = prim.uvD;

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::Kernels::rtps();

        quadT.setColorA(interpColorImm(textureNeutral));

        uint32_t pa = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v1.pos);
        psyqo::GTE::Kernels::rtps();

        quadT.setColorB(interpColorImm(textureNeutral));

        uint32_t pb = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v2.pos);
        psyqo::GTE::Kernels::rtps();

        quadT.setColorC(interpColorImm(textureNeutral));

        uint32_t pc = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        quadT.setColorD(interpColorImm(textureNeutral));

        uint32_t pd = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += floorBias;

        if (avgZ >= Renderer::OT_SIZE) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadT.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadT.pointD.packed);

        auto& quadFragFog = primBuffer.allocateFragment<psyqo::Prim::GouraudQuad>();
        auto& quadFog = quadFragFog.primitive;

        quadFog.setColorA(interpColorBack(fogColor, pa));
        quadFog.setColorB(interpColorBack(fogColor, pb));
        quadFog.setColorC(interpColorBack(fogColor, pc));
        quadFog.setColorD(interpColorBack(fogColor, pd));

        quadFog.pointA = quadT.pointA;
        quadFog.pointB = quadT.pointB;
        quadFog.pointC = quadT.pointC;
        quadFog.pointD = quadT.pointD;

        ot.insert(quadFragT, avgZ);
        ot.insert(quadFragFog, avgZ);
    }
}

void Renderer::drawTileMesh(TileIndex tileIndex,
    const Tile& tile,
    const Tileset& tileset,
    const MeshData& meshData,
    const Camera& camera)
{
    const int x = tileIndex.x;
    const int z = tileIndex.z;

    auto& ot = getOrderingTable();
    auto& primBuffer = getPrimBuffer();

    // mostly like drawMeshStatic, but moves each vertex into camera space without
    // using rotation/translation registers
    // - FIXME: remove copy-paste
    const auto& g3s = meshData.g3;
    const auto& g4s = meshData.g4;
    const auto& gt3s = meshData.gt3;
    const auto& gt4s = meshData.gt4;
    const auto& verts = meshData.vertices;

    const auto g4Offset = g3s.size() * 3;
    const auto gt3Offset = g4Offset + g4s.size() * 4;
    const auto gt4Offset = gt3Offset + gt3s.size() * 3;

    const auto& tileInfo = tileset.getTileInfo(tile.tileId);
    const auto tileHeight = psyqo::FixedPoint<>(tileInfo.height);

    const auto originX =
        psyqo::GTE::Short(psyqo::FixedPoint<>(x, 0) * Tile::SCALE - camera.position.x);
    const auto originY = psyqo::GTE::Short(tileHeight - camera.position.y);
    const auto originZ =
        psyqo::GTE::Short(psyqo::FixedPoint<>(z, 0) * Tile::SCALE - camera.position.z);

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

        auto& quadFragT = primBuffer.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        auto& quadT = quadFragT.primitive;
        quadT.tpage = prim.tpage;
        quadT.clutIndex = prim.clutIndex;
        quadT.uvA = prim.uvA;
        quadT.uvB = prim.uvB;
        quadT.uvC = prim.uvC;
        quadT.uvD = prim.uvD;

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v0.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(v1.pos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2.pos);
        psyqo::GTE::Kernels::rtpt();

        // do while rtpt is doing this
        quadT.setColorA(textureNeutral);
        quadT.setColorB(textureNeutral);
        quadT.setColorC(textureNeutral);
        quadT.setColorD(textureNeutral);

        psyqo::GTE::Kernels::nclip();
        const auto dot =
            (int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0, psyqo::GTE::Safe>();
        if (dot < 0) {
            continue;
        }

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointA.packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3.pos);
        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::Kernels::avsz4();

        auto avgZ = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ, psyqo::GTE::Safe>();
        if (avgZ == 0) { // cull
            continue;
        }

        avgZ += floorBias;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&quadT.pointB.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&quadT.pointC.packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&quadT.pointD.packed);

        ot.insert(quadFragT, avgZ);
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

void Renderer::drawLineWorldSpace(const Camera& camera,
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

void Renderer::drawPointWorldSpace(const Camera& camera,
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
    psyqo::GTE::Math::matrixVecMul3<psyqo::GTE::PseudoRegister::Rotation,
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
    psyqo::GTE::Math::matrixVecMul3<psyqo::GTE::PseudoRegister::Rotation,
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
    psyqo::GTE::Math::matrixVecMul3<psyqo::GTE::PseudoRegister::Rotation,
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

void Renderer::drawArmature(const Armature& armature,
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

    const auto jointColor = (joint.id != armature.selectedJoint) ?
                                psyqo::Color{.r = 255, .g = 255, .b = 128} :
                                psyqo::Color{.r = 255, .g = 255, .b = 255};
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
    psyqo::Rect region = {.pos = {{.x = (std::int16_t)tim.pixDX, .y = (std::int16_t)tim.pixDY}},
        .size = {{.w = (std::int16_t)tim.pixW, .h = (std::int16_t)tim.pixH}}};
    gpu.uploadToVRAM((uint16_t*)tim.pixelsIdx.data(), region);

    // upload CLUT(s)
    // FIXME: support multiple cluts
    region = {.pos = {{.x = (std::int16_t)tim.clutDX, .y = (std::int16_t)tim.clutDY}},
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
