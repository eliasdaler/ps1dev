#pragma once

#include <concepts>
#include <cstdint>

using ssize_t = std::int32_t; // TODO: remove after updating psyqo
#include <EASTL/variant.h>

#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#include <psyqo/gte-registers.hh>
#include <psyqo/vector.hh>

#include <psyqo/fragments.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>

#include "Armature.h"

struct Vec3Pad {
    psyqo::GTE::PackedVec3 pos;
    std::uint16_t pad;
};

namespace util
{
struct FileReader;
}

template<typename PrimType>
using FragData = eastl::vector<PrimType>;

struct Mesh;

struct MeshData {
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

    Mesh makeInstance() const;
};

struct Mesh {
    const MeshData* meshData{nullptr};
};

struct Model;

struct ModelData {
    eastl::vector<MeshData> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
    void load(util::FileReader& fr);

    Model makeInstance() const;
};

struct Model {
    eastl::vector<Mesh> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
};
