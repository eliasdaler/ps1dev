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

struct PsxModel {
    std::vector<PsxSubmesh> submeshes;
};

void writePsxModel(const PsxModel& model, const std::filesystem::path& path);
