#include "Level.h"

#include <Core/FileReader.h>

#include <common/syscalls/syscalls.h>

void Level::load(const eastl::vector<uint8_t>& data)
{
    ramsyscall_printf("Load level: %d bytes\n", (int)data.size());

    usedTextures.clear();
    usedModels.clear();
    collisionBoxes.clear();

    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numUsedTextures = fr.GetUInt16();
    usedTextures.reserve(numUsedTextures);
    ramsyscall_printf("num used textures: %d\n", numUsedTextures);
    for (int i = 0; i < numUsedTextures; ++i) {
        StringHash filename{.value = fr.GetUInt32()};
        usedTextures.push_back(filename);
    }

    const auto numUsedModels = fr.GetUInt16();
    usedModels.reserve(numUsedModels);
    ramsyscall_printf("num used models: %d\n", numUsedModels);
    for (int i = 0; i < numUsedModels; ++i) {
        StringHash filename{.value = fr.GetUInt32()};
        usedModels.push_back(filename);
    }

    const auto numColliders = fr.GetUInt16();
    collisionBoxes.reserve(numColliders);
    ramsyscall_printf("num colliders: %d\n", numColliders);

    for (int i = 0; i < numColliders; ++i) {
        AABB aabb;

        aabb.min = psyqo::Vec3{
            .x = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
            .y = 0.0,
            .z = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
        };

        aabb.max = psyqo::Vec3{
            .x = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
            .y = 0.1,
            .z = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
        };

        collisionBoxes.push_back(eastl::move(aabb));
    }
}
