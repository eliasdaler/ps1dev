#pragma once

#include <cstdint>

#include <EASTL/unique_ptr.h>
#include <EASTL/span.h>
#include <EASTL/string_view.h>

#include <libgpu.h>

struct FastVertex {
    std::int16_t x, y, z, w;
};

struct FastModelInstance;

struct FastModel {
    eastl::unique_ptr<FastVertex> vertexData{nullptr};
    std::uint16_t numVertices{0};
    eastl::unique_ptr<std::byte> primData{nullptr};
    std::uint16_t numTris{0};
    std::uint16_t numQuads{0};

    // functions
    void load(eastl::string_view filename);
    FastModelInstance makeFastModelInstance(const TIM_IMAGE& texture);
};

struct FastModelInstance {
    eastl::span<FastVertex> vertices;
    eastl::unique_ptr<std::byte> primData{nullptr};
    eastl::span<POLY_GT3> trianglePrims[2];
    eastl::span<POLY_GT4> quadPrims[2];
};
