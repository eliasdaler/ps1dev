#pragma once

#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#include <psyqo/gte-registers.hh>
#include <psyqo/vector.hh>

#include <psyqo/fragments.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>

#include <cstdint>

#include "Armature.h"

struct Object;

struct CVECTOR {
    std::uint8_t vx, vy, vz, pad;
};

struct Vertex {
    psyqo::GTE::PackedVec3 pos;
    std::uint16_t pad;

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
};

struct Model {
    eastl::vector<Mesh> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
};

struct Vec3Pad {
    psyqo::GTE::PackedVec3 pos;
    std::uint16_t pad;
};

template<typename PrimType>
using FragData = eastl::array<eastl::vector<psyqo::Fragments::SimpleFragment<PrimType>>, 2>;

struct FastMesh {
    int numUntexturedTris{0};
    int numUntexturedQuads{0};
    int numTris{0};
    int numQuads{0};
    std::uint16_t jointId;

    eastl::vector<Vec3Pad> vertices;

    FragData<psyqo::Prim::GouraudTriangle> g3;
    FragData<psyqo::Prim::GouraudQuad> g4;
    FragData<psyqo::Prim::GouraudTexturedTriangle> gt3;
    FragData<psyqo::Prim::GouraudTexturedQuad> gt4;
};

struct FastModel {
    eastl::vector<FastMesh> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
};
