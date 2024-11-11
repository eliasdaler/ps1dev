#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "Math.h"

struct FastVertex {
    Vec4<std::int16_t> pos;
    Vec4<std::uint8_t> color;
};

struct FastModel {
    std::vector<FastVertex> vertexData;
    std::uint16_t triNum;
    std::uint16_t quadNum;
    std::vector<std::uint8_t> primData;
};

void writeFastModel(const FastModel& model, const std::filesystem::path& path);
