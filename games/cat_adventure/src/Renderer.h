#pragma once

#include <EASTL/array.h>

#include <psyqo/bump-allocator.h>
#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/trigonometry.hh>

#include "Model.h"
#include "TextureInfo.h"

struct ModelObject;
struct MeshObject;
struct Camera;

class Renderer {
public:
    Renderer(psyqo::GPU& gpu);

    void init();

    void drawModelObject(
        const ModelObject& object,
        const Camera& camera,
        const TextureInfo& texture,
        bool setViewRot = true);

    void drawModelObject(
        const ModelObject& object,
        const Armature& armature,
        const Camera& camera,
        const TextureInfo& texture,
        bool setViewRot = true);

    void drawMeshObject(const MeshObject& object, const Camera& camera, const TextureInfo& texture);

    void drawModel(const Model& model, const TextureInfo& texture);
    void drawMesh(const Mesh& mesh, const TextureInfo& texture);

    static constexpr auto OT_SIZE = 4096 * 2;
    using OrderingTableType = psyqo::OrderingTable<OT_SIZE>;
    eastl::array<OrderingTableType, 2> ots;

    static constexpr int PRIMBUFFLEN = 32768 * 8;
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
        const psyqo::Color& c);

    void setFogNearFar(psyqo::FixedPoint<> near, psyqo::FixedPoint<> far);
    void setFarColor(const psyqo::Color& c);
    uint32_t calcInterpFactor(uint32_t sz);

    void setFOV(uint32_t nh);

private:
    bool shouldCullObject(const Object& object, const Camera& camera) const;

    psyqo::GPU& gpu;
    psyqo::Trig<> trig;

    template<typename PrimType>
    void drawTris(
        const Mesh& mesh,
        const TextureInfo& texture,
        int numFaces,
        std::size_t& outVertIdx);

    template<typename PrimType>
    void drawQuads(
        const Mesh& mesh,
        const TextureInfo& texture,
        int numFaces,
        std::size_t& outVertIdx);

    uint32_t dqa{};
    uint32_t dqb{};
    uint32_t h{300};
};
