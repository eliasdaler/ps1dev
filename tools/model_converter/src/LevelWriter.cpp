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
        fsutil::binaryWrite(file, pad16);
    }
}
