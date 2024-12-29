#pragma once

#include <filesystem>
#include <vector>

struct ConversionParams;
struct ModelJson;
struct LevelJson;

void writeLevelToFile(
    std::filesystem::path& path,
    const ModelJson& model,
    const LevelJson& level,
    const ConversionParams& conversionParams);
