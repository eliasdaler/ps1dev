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
    void drawModelObject(
        const psyqo::GPU& gpu,
        const ModelObject& object,
        const Camera& camera,
        const TextureInfo& texture);
    void drawModel(const psyqo::GPU& gpu, const Model& model, const TextureInfo& texture);
    void drawMesh(const psyqo::GPU& gpu, const Mesh& mesh, const TextureInfo& texture);

    static constexpr auto OT_SIZE = 4096 * 2;
    eastl::array<psyqo::OrderingTable<OT_SIZE>, 2> ots;

    static constexpr int PRIMBUFFLEN = 32768 * 8;
    using PrimBufferAllocatorType = psyqo::BumpAllocator<PRIMBUFFLEN>;
    eastl::array<PrimBufferAllocatorType, 2> primBuffers;

    PrimBufferAllocatorType& getPrimBuffer(const psyqo::GPU& gpu)
    {
        return primBuffers[gpu.getParity()];
    }

private:
    psyqo::Trig<> trig;

    template<typename PrimType>
    void drawTris(
        const psyqo::GPU& gpu,
        const Mesh& mesh,
        const TextureInfo& texture,
        int numFaces,
        std::size_t& outVertIdx);

    template<typename PrimType>
    void drawQuads(
        const psyqo::GPU& gpu,
        const Mesh& mesh,
        const TextureInfo& texture,
        int numFaces,
        std::size_t& outVertIdx);
};
