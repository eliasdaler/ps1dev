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
using FragData = eastl::vector<psyqo::Fragments::SimpleFragment<PrimType>>;

template<typename PrimType>
using FragDataDoubleBuffer = eastl::array<FragData<PrimType>, 2>;

struct Mesh;
struct MeshUnique;

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
    MeshUnique makeInstanceUnique();
};

template<typename T>
concept RenderableMesh = requires(T mesh, int parity) {
    { mesh.getJointId() } -> std::same_as<std::uint16_t>;
    { mesh.getG3s(parity) } -> std::same_as<FragData<psyqo::Prim::GouraudTriangle>&>;
    { mesh.getG4s(parity) } -> std::same_as<FragData<psyqo::Prim::GouraudQuad>&>;
    { mesh.getGT3s(parity) } -> std::same_as<FragData<psyqo::Prim::GouraudTexturedTriangle>&>;
    { mesh.getGT4s(parity) } -> std::same_as<FragData<psyqo::Prim::GouraudTexturedQuad>&>;
    { mesh.getVertices() } -> std::same_as<const eastl::vector<Vec3Pad>&>;
};

struct Mesh {
    std::uint16_t getJointId() const { return jointId; }
    const eastl::vector<Vec3Pad>& getVertices() const { return *vertices; }

    FragData<psyqo::Prim::GouraudTriangle>& getG3s(int parity) { return g3[parity]; }
    FragData<psyqo::Prim::GouraudQuad>& getG4s(int parity) { return g4[parity]; }
    FragData<psyqo::Prim::GouraudTexturedTriangle>& getGT3s(int parity) { return gt3[parity]; }
    FragData<psyqo::Prim::GouraudTexturedQuad>& getGT4s(int parity) { return gt4[parity]; }

    // data
    std::uint16_t jointId;

    const eastl::vector<Vec3Pad>* vertices{nullptr}; // reference to Mesh.vertices

    FragDataDoubleBuffer<psyqo::Prim::GouraudTriangle> g3;
    FragDataDoubleBuffer<psyqo::Prim::GouraudQuad> g4;
    FragDataDoubleBuffer<psyqo::Prim::GouraudTexturedTriangle> gt3;
    FragDataDoubleBuffer<psyqo::Prim::GouraudTexturedQuad> gt4;
};

static_assert(RenderableMesh<Mesh>);

struct MeshUnique {
    std::uint16_t getJointId() const { return meshData.jointId; }
    const eastl::vector<Vec3Pad>& getVertices() const { return meshData.vertices; }

    FragData<psyqo::Prim::GouraudTriangle>& getG3s(int parity)
    {
        if (parity == 0) {
            return meshData.g3;
        }
        return g3;
    }

    FragData<psyqo::Prim::GouraudQuad>& getG4s(int parity)
    {
        if (parity == 0) {
            return meshData.g4;
        }
        return g4;
    }

    FragData<psyqo::Prim::GouraudTexturedTriangle>& getGT3s(int parity)
    {
        if (parity == 0) {
            return meshData.gt3;
        }
        return gt3;
    }

    FragData<psyqo::Prim::GouraudTexturedQuad>& getGT4s(int parity)
    {
        if (parity == 0) {
            return meshData.gt4;
        }
        return gt4;
    }

    // data
    MeshData& meshData;

    FragData<psyqo::Prim::GouraudTriangle> g3;
    FragData<psyqo::Prim::GouraudQuad> g4;
    FragData<psyqo::Prim::GouraudTexturedTriangle> gt3;
    FragData<psyqo::Prim::GouraudTexturedQuad> gt4;
};

static_assert(RenderableMesh<MeshUnique>);

struct Model;

struct ModelData {
    eastl::vector<MeshData> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
    void load(util::FileReader& fr);

    Model makeInstance() const;
    Model makeInstanceUnique();
};

struct Model {
    eastl::vector<eastl::variant<Mesh, MeshUnique>> meshes;
    Armature armature;

    void load(const eastl::vector<uint8_t>& data);
};
