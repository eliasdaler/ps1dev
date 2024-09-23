#pragma once

#include <cstdint>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

struct Vertex {
    std::int16_t x, y, z;
    std::uint8_t u, v;
    std::uint8_t r, g, b, pad;
};

struct Mesh {
    eastl::vector<Vertex> vertices;
    int numTris{0};
    int numQuads{0};
};

struct Model {
    eastl::vector<Mesh> meshes;
};

Model loadModel(eastl::string_view filename);
