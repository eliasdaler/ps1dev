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
struct Camera;

class Renderer {
public:
    Renderer(psyqo::GPU& gpu);

    void drawModelObject(
        const ModelObject& object,
        const Camera& camera,
        const TextureInfo& texture);
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

private:
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
};
