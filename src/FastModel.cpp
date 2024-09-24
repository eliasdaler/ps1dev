#include "FastModel.h"
#include "Utils.h"

#include <psyqo/alloc.h>

void FastModel::load(eastl::string_view filename)
{
    const auto data = util::readFile(filename);
    const auto* bytes = (u_char*)data.data();

    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    // get num vertices
    numVertices = fr.GetUInt16();

    // load vertices
    const auto vertDataSize = numVertices * sizeof(FastVertex);
    vertexData = eastl::unique_ptr<FastVertex>((FastVertex*)psyqo_malloc(vertDataSize));
    fr.GetBytes(vertexData.get(), vertDataSize);

    // get num of tris and quads
    numTris = fr.GetUInt16();
    numQuads = fr.GetUInt16();

    // read prim data
    const auto primDataSize = numTris * sizeof(POLY_GT3) + numQuads * sizeof(POLY_GT4);
    primData = eastl::unique_ptr<std::byte>((std::byte*)psyqo_malloc(primDataSize));
    fr.GetBytes(primData.get(), primDataSize);
}

FastModelInstance FastModel::makeFastModelInstance(const TIM_IMAGE& texture)
{
    FastModelInstance instance{
        .vertices = eastl::span{vertexData.get(), vertexData.get() + numVertices},
    };
    const auto primDataSize = numTris * sizeof(POLY_GT3) + numQuads * sizeof(POLY_GT4);

    instance.primData = eastl::unique_ptr<std::byte>((std::byte*)psyqo_malloc(2 * primDataSize));

    memcpy((void*)instance.primData.get(), primData.get(), primDataSize);
    // copy again
    memcpy((void*)(instance.primData.get() + primDataSize), primData.get(), primDataSize);

    for (int i = 0; i < 2; ++i) {
        auto* trisStart = instance.primData.get() + i * primDataSize;
        auto* trisEnd = trisStart + numTris * sizeof(POLY_GT3);

        instance.trianglePrims[i] = eastl::span<POLY_GT3>((POLY_GT3*)trisStart, (POLY_GT3*)trisEnd);
        instance.quadPrims[i] = eastl::span<
            POLY_GT4>((POLY_GT4*)trisEnd, (POLY_GT4*)(trisEnd + numQuads * sizeof(POLY_GT4)));
    }

    // set texture
    // FIXME: potentially do this in loadModel? (requires to make spans for prims, though)
    const auto tpage = getTPage(texture.mode & 0x3, 0, texture.prect->x, texture.prect->y);
    const auto clut = getClut(texture.crect->x, texture.crect->y);
    for (int i = 0; i < 2; ++i) {
        for (auto& prim : instance.trianglePrims[i]) {
            prim.tpage = tpage;
            prim.clut = clut;
        }
        for (auto& prim : instance.quadPrims[i]) {
            prim.tpage = tpage;
            prim.clut = clut;
        }
    }

    return instance;
}
