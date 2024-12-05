#pragma once

#include "Math.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

struct PsxVert {
    Vec3<std::int16_t> pos;
    Vec2<std::uint8_t> uv;
    Vec3<std::uint8_t> color;
    std::uint16_t originalIndex;
    // not written into binary file, but useful for finding the vertex in the mesh
};

using PsxTriFace = std::array<PsxVert, 3>;
using PsxQuadFace = std::array<PsxVert, 4>;

struct PsxSubmesh {
    bool subdivide{false};
    std::vector<PsxTriFace> untexturedTriFaces;
    std::vector<PsxQuadFace> untexturedQuadFaces;
    std::vector<PsxTriFace> triFaces;
    std::vector<PsxQuadFace> quadFaces;
};

struct PsxJoint {
    using JointId = std::uint8_t;
    static constexpr JointId NULL_JOINT_ID = 0xFF;

    Vec3<std::int16_t> translation;
    Vec4<std::int16_t> rotation;
    JointId firstChild{NULL_JOINT_ID};
    JointId nextSibling{NULL_JOINT_ID};
    std::uint16_t boneInfluencesOffset;
    std::uint16_t boneInfluencesSize;
};

struct PsxArmature {
    std::vector<PsxJoint> joints;
    std::vector<std::uint16_t> boneInfluences;
};

struct PsxModel {
    std::vector<PsxSubmesh> submeshes;
    PsxArmature armature;
};

void writePsxModel(const PsxModel& model, const std::filesystem::path& path);
