#include "LevelJsonFile.h"

#include <format>
#include <fstream>

#include <nlohmann/json.hpp>

namespace
{
std::uint8_t getUInt8(const nlohmann::json& obj, const char* key)
{
    auto v = obj.at(key).get<int>();
    if (v < 0 || v > 256) {
        throw std::runtime_error(std::format("bad value for property {} in tile object", key));
    }
    return v;
}
}

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

    if (root.contains("tileset")) {
        const auto& tilesetObj = root.at("tileset");
        level.tileset.reserve(tilesetObj.size());
        for (const auto& tileObj : tilesetObj) {
            TileInfo ti;
            if (tileObj.contains("name")) {
                ti.name = tileObj.at("name").get<std::string>();
            }

            ti.id = getUInt8(tileObj, "id");
            if (tileObj.contains("u0")) {
                ti.u0 = getUInt8(tileObj, "u0");
                ti.v0 = getUInt8(tileObj, "v0");
                ti.u1 = getUInt8(tileObj, "u1");
                ti.v1 = getUInt8(tileObj, "v1");
            }
            if (tileObj.contains("model_id")) {
                ti.modelId = getUInt8(tileObj, "model_id");
            }

            if (tileObj.contains("height")) {
                ti.height = tileObj.at("height").get<float>();
            }

            level.tileset.push_back(std::move(ti));
        }
    }

    return level;
}
