#include "LevelWriter.h"

#include <FsUtil.h>
#include <fstream>

#include "ConversionParams.h"
#include "FixedPoint.h"
#include "LevelJsonFile.h"
#include "ModelJsonFile.h"
#include "PsxModel.h"

#include "DJBHash.h"

static const std::uint8_t pad8{0};
static const std::uint16_t pad16{0};

void writeLevelToFile(
    std::filesystem::path& path,
    const ModelJson& model,
    const LevelJson& level,
    const ConversionParams& conversionParams)
{
    std::ofstream file(path, std::ios::binary);

    // write used textures
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(level.usedTextures.size()));
    for (const auto& filename : level.usedTextures) {
        fsutil::binaryWrite(file, DJBHash::hash(filename));
    }

    // write used models
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(level.usedModels.size()));
    for (const auto& filename : level.usedModels) {
        fsutil::binaryWrite(file, DJBHash::hash(filename));
    }

    // write collision
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(model.collision.size()));
    for (const auto& coll : model.collision) {
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.min.x, conversionParams.scale));
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.min.z, conversionParams.scale));
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.max.x, conversionParams.scale));
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.max.z, conversionParams.scale));
    }
}

void writeLevelToFileNew(
    std::filesystem::path& path,
    const ModelJson& model,
    const PsxModel& psxModel,
    const LevelJson& level,
    const ConversionParams& conversionParams)
{
    std::ofstream file(path, std::ios::binary);

    // write used textures
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(level.usedTextures.size()));
    for (const auto& filename : level.usedTextures) {
        fsutil::binaryWrite(file, DJBHash::hash(filename));
    }

    // write used models
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(level.usedModels.size()));
    for (const auto& filename : level.usedModels) {
        fsutil::binaryWrite(file, DJBHash::hash(filename));
    }

    writePsxModel(psxModel, file);

    fsutil::binaryWrite(file, static_cast<std::uint32_t>(model.objects.size()));
    for (const auto& object : model.objects) {
        fsutil::binaryWrite(
            file, floatToFixed<std::int32_t>(object.transform.position.x, conversionParams.scale));
        fsutil::binaryWrite(
            file, floatToFixed<std::int32_t>(object.transform.position.y, conversionParams.scale));
        fsutil::binaryWrite(
            file, floatToFixed<std::int32_t>(object.transform.position.z, conversionParams.scale));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(object.mesh));

        const auto euler = glm::eulerAngles(object.transform.rotation);

        if (euler.x != 0.0 || euler.y != 0.f) {
            printf(
                "[WARNING] object has non-yaw rotation, name: %s, x: %.2f, y: %.2f, z: %.2f\n",
                object.name.c_str(),
                glm::degrees(euler.x),
                glm::degrees(euler.y),
                glm::degrees(euler.z));
        }

        // write yaw in "half turns", the format which psyqo uses
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(euler.z / (M_PI)));
    }

    // write tiles
    fsutil::binaryWrite(file, static_cast<std::uint32_t>(level.tileset.size()));
    for (const auto& tile : level.tileset) {
        std::uint8_t tileFlags{};
        tileFlags |= (tile.modelId != TileInfo::NULL_MODEL_ID);
        fsutil::binaryWrite(file, tileFlags);

        fsutil::binaryWrite(file, tile.id);

        if (tile.modelId == TileInfo::NULL_MODEL_ID) {
            fsutil::binaryWrite(file, tile.u0);
            fsutil::binaryWrite(file, tile.v0);
            fsutil::binaryWrite(file, tile.u1);
            fsutil::binaryWrite(file, tile.v1);
        } else {
            fsutil::binaryWrite(file, tile.modelId);
        }

        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(tile.height));
    }

    // write collision
    fsutil::binaryWrite(file, static_cast<std::uint32_t>(model.collision.size()));
    for (const auto& coll : model.collision) {
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.min.x, conversionParams.scale));
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.min.z, conversionParams.scale));
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.max.x, conversionParams.scale));
        fsutil::binaryWrite(file, floatToFixed<std::int16_t>(coll.max.z, conversionParams.scale));
    }
}
