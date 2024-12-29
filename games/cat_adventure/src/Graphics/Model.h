#pragma once

#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#include <psyqo/gte-registers.hh>
#include <psyqo/vector.hh>

#include <psyqo/fragments.hh>
#include <psyqo/primitives/quads.hh>

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

struct GT4Data {
    psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedQuad> frag;

    struct Vec3Pad {
        psyqo::GTE::PackedVec3 pos;
        std::uint16_t pad;
    };

    eastl::array<Vec3Pad, 4> vs;
};

struct FastMesh {
    int numUntexturedTris{0};
    int numUntexturedQuads{0};
    int numTris{0};
    int numQuads{0};
    std::uint16_t jointId;

    eastl::vector<GT4Data> gt4;
};

struct FastModel {
    eastl::vector<FastMesh> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
};
