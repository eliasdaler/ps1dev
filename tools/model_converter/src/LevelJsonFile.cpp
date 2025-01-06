#include "LevelJsonFile.h"

#include <fstream>
#include <nlohmann/json.hpp>

LevelJson parseLevelJsonFile(
    const std::filesystem::path& path,
    const std::filesystem::path& assetDirPath)
{
    nlohmann::json root;
    std::ifstream file(path);
    assert(file.good());
    file >> root;

    LevelJson level{};
    if (root.contains("used_textures")) {
        level.usedTextures = root.at("used_textures");
    }
    if (root.contains("used_models")) {
        level.usedModels = root.at("used_models");
    }

    return level;
}
