#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct LevelJson {
    std::vector<std::string> usedTextures;
    std::vector<std::string> usedModels;
};

LevelJson parseLevelJsonFile(
    const std::filesystem::path& path,
    const std::filesystem::path& assetDirPath);
