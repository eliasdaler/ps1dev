#include "LevelWriter.h"

#include <FsUtil.h>
#include <fstream>

#include "ConversionParams.h"
#include "FixedPoint.h"
#include "LevelJsonFile.h"
#include "ModelJsonFile.h"

#include "DJBHash.h"

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
