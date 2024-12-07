#pragma once

#include <filesystem>
#include <vector>

struct Animation;
struct ConversionParams;

void writeAnimationsToFile(
    std::filesystem::path& path,
    const std::vector<Animation>& animations,
    const ConversionParams& conversionParams);
