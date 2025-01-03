#pragma once

#include <EASTL/array.h>

#include <psyqo/bump-allocator.h>
#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/trigonometry.hh>

#include <Graphics/Model.h>
#include <Graphics/TextureInfo.h>

struct ModelObject;
struct MeshObject;
struct AnimatedModelObject;
struct Camera;
struct Circle;
struct AABB;
struct TimFile;

class Renderer {
public:
    Renderer(psyqo::GPU& gpu);

    void init();

    [[nodiscard]] TextureInfo uploadTIM(const TimFile& tim);

    void drawModelObject(const ModelObject& object, const Camera& camera, bool setViewRot = true);

    void drawAnimatedModelObject(
        const AnimatedModelObject& object,
        const Camera& camera,
        bool setViewRot = true);
    void drawAnimatedModelObject2(
        const AnimatedModelObject& object,
        const Camera& camera,
        bool setViewRot = true);

    void drawMeshObject(const MeshObject& object, const Camera& camera);

    void drawModel(const Model& model, const TextureInfo& texture);
    void drawModel(FastModel& model);

    void drawMesh(const Mesh& mesh, const TextureInfo& texture);
    void drawMesh(const Mesh& mesh);
    void drawMesh(FastMesh& mesh);

    static constexpr auto OT_SIZE = 4096 * 2;
    using OrderingTableType = psyqo::OrderingTable<OT_SIZE>;
    eastl::array<OrderingTableType, 2> ots;

    // TODO: can make much smaller once static geometry stores primitives on heap
    static constexpr int PRIMBUFFLEN = 32768 * 2;
    using PrimBufferAllocatorType = psyqo::BumpAllocator<PRIMBUFFLEN>;
    eastl::array<PrimBufferAllocatorType, 2> primBuffers;

    OrderingTableType& getOrderingTable() { return ots[gpu.getParity()]; }
    PrimBufferAllocatorType& getPrimBuffer() { return primBuffers[gpu.getParity()]; }

    psyqo::GPU& getGPU() { return gpu; }

    void calculateViewModelMatrix(
        const Object& object,
        const Camera& camera,
        bool setViewRot = true);

    int bias{0};

    void drawObjectAxes(const Object& object, const Camera& camera);

    /* This function assumes that V*M is already loaded into R and T, e.g. call
     * renderer.calculateViewModelMatrix(object, camera, true); before calling this
     */
    void drawLineLocalSpace(const psyqo::Vec3& a, const psyqo::Vec3& b, const psyqo::Color& c);

    /*
     * This function is kinda limited - can only draw lines that are ~16
     * units long. But they can start and end at any world coord.
     */
    void drawLineWorldSpace(
        const Camera& camera,
        const psyqo::Vec3& a,
        const psyqo::Vec3& b,
        const psyqo::Color& c,
        bool cameraViewLoaded = false);

    void drawAABB(const Camera& camera, const AABB& aabb, const psyqo::Color& c);

    void drawCircle(const Camera& camera, const Circle& circle, const psyqo::Color& c);

    void drawArmature(const AnimatedModelObject& object, const Camera& camera);

    void setFogNearFar(psyqo::FixedPoint<> near, psyqo::FixedPoint<> far);
    void setFarColor(const psyqo::Color& c);
    uint32_t calcInterpFactor(uint32_t sz);

    void setFOV(uint32_t nh);

    void setFogEnabled(bool b) { fogEnabled = b; };

private:
    bool shouldCullObject(const Object& object, const Camera& camera) const;

    psyqo::GPU& gpu;
    psyqo::Trig<> trig;

    template<typename PrimType, bool fogEnabledT>
    void drawTris(
        const Mesh& mesh,
        const TextureInfo& texture,
        int numFaces,
        std::size_t& outVertIdx);

    template<typename PrimType, bool fogEnabledT>
    void drawQuads(
        const Mesh& mesh,
        const TextureInfo& texture,
        int numFaces,
        std::size_t& outVertIdx);

    void drawArmature(
        const Armature& armature,
        const AnimatedModelObject& object,
        const Joint& joint,
        Joint::JointId childId);

    std::uint32_t dqa{};
    std::uint32_t dqb{};
    std::uint32_t h{300};

    bool fogEnabled{true};
};
