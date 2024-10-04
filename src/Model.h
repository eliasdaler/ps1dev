#pragma once

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <psyqo/vector.hh>

struct Object;

struct SVECTOR {
    short vx, vy, vz, pad;
};

struct CVECTOR {
    char vx, vy, vz, pad;
};

struct Vertex {
    SVECTOR pos;
    CVECTOR uv;
    CVECTOR col;
};

struct Mesh {
    eastl::vector<Vertex> vertices;
    int numUntexturedTris{0};
    int numUntexturedQuads{0};
    int numTris{0};
    int numQuads{0};
    bool subdivide{false}; // TODO: flags std::uint16_t
};

struct Model {
    eastl::vector<Mesh> meshes;

    void load(const eastl::vector<uint8_t>& data);
};
