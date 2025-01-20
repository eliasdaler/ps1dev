#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct TileInfo {
    std::string name; // not saved to file - mostly for debug/comment

    std::uint8_t id;

    std::uint8_t u0, v0; // top-left
    std::uint8_t u1, v1; // bottom-right

    static constexpr uint8_t NULL_MODEL_ID = 0xFF;
    std::uint8_t modelId{NULL_MODEL_ID}; // modelData.meshes[modelId]

    float height{0.f};
};

struct LevelJson {
    std::vector<std::string> usedTextures;
    std::vector<std::string> usedModels;
    std::vector<TileInfo> tileset;
};

LevelJson parseLevelJsonFile(
    const std::filesystem::path& path,
    const std::filesystem::path& assetDirPath);
