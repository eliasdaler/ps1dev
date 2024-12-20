#pragma once

#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#include <psyqo/gte-registers.hh>
#include <psyqo/vector.hh>

#include <cstdint>

#include "Armature.h"

struct Object;

struct CVECTOR {
    std::uint8_t vx, vy, vz, pad;
};

struct Vertex {
    psyqo::GTE::PackedVec3 pos;
    std::int16_t pad;

    CVECTOR uv;
    psyqo::Color col;
};

struct Mesh {
    eastl::vector<Vertex> vertices;
    int numUntexturedTris{0};
    int numUntexturedQuads{0};
    int numTris{0};
    int numQuads{0};
    std::uint16_t jointId;

    eastl::vector<Vertex> ogVertices;
};

struct Model {
    eastl::vector<Mesh> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
};
