#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "Math.h"

constexpr int POS_ATTR = 0;
constexpr int UV_ATTR = 1;
constexpr int NORMAL_ATTR = 2;

struct ObjFace {
    using VertexIndices = std::array<int, 4>;
    std::array<VertexIndices, 4> indices;
    int numVertices;
    int numAttrs;
};

struct ObjMesh {
    std::string name;
    std::vector<ObjFace> faces;
};

struct ObjModel {
    std::vector<Vec3<float>> vertices;
    std::vector<Vec3<float>> normals;
    std::vector<Vec2<float>> uvs;
    std::vector<ObjMesh> meshes;
};

ObjModel parseObjFile(const std::filesystem::path& path);
