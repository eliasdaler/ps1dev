#pragma once

#include "Math.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

using FixedPoint4_12 = std::int16_t;
using FixedPoint20_12 = std::int32_t;

struct PsxVert {
    Vec3<FixedPoint4_12> pos;
    Vec2<std::uint8_t> uv;
    Vec3<std::uint8_t> color;
    std::uint16_t originalIndex;
    // not written into binary file, but useful for finding the vertex in the mesh
};

using PsxTriFace = std::array<PsxVert, 3>;
using PsxQuadFace = std::array<PsxVert, 4>;

struct PsxSubmesh {
    bool subdivide{false};
    int jointId{-1};
    std::vector<PsxTriFace> untexturedTriFaces;
    std::vector<PsxQuadFace> untexturedQuadFaces;
    std::vector<PsxTriFace> triFaces;
    std::vector<PsxQuadFace> quadFaces;
};

struct PsxJoint {
    using JointId = std::uint8_t;
    static constexpr JointId NULL_JOINT_ID = 0xFF;

    Vec3<FixedPoint4_12> translation;
    Vec4<FixedPoint4_12> rotation;
    JointId firstChild{NULL_JOINT_ID};
    JointId nextSibling{NULL_JOINT_ID};
};

struct PsxMatrix {
    std::array<FixedPoint4_12, 9> rotation;
    std::int16_t pad;
    Vec3<FixedPoint20_12> translation;
};

struct PsxArmature {
    std::vector<PsxJoint> joints;
    std::vector<PsxMatrix> inverseBindMatrices;
};

struct PsxModel {
    std::vector<PsxSubmesh> submeshes;
    PsxArmature armature;
};

void writePsxModel(const PsxModel& model, const std::filesystem::path& path);
