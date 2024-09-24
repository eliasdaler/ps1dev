#pragma once

#include <cstdint>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <libgpu.h>

struct Object;

struct Vertex {
    SVECTOR pos;
    CVECTOR uv;
    CVECTOR col;
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
