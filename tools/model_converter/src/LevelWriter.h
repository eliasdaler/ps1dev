#pragma once

#include <filesystem>
#include <vector>

struct ConversionParams;
struct ModelJson;
struct LevelJson;
struct PsxModel;

void writeLevelToFile(
    std::filesystem::path& path,
    const ModelJson& model,
    const LevelJson& level,
    const ConversionParams& conversionParams);

void writeLevelToFileNew(
    std::filesystem::path& path,
    const ModelJson& model,
    const PsxModel& psxModel,
    const LevelJson& level,
    const ConversionParams& conversionParams);
