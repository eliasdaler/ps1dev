#include "Level.h"

#include <Core/FileReader.h>

#include <common/syscalls/syscalls.h>

void Level::load(const eastl::vector<uint8_t>& data)
{
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

void Level::loadNewFormat(const eastl::vector<uint8_t>& data)
{
    usedTextures.clear();
    usedModels.clear();
    collisionBoxes.clear();
    triggers.clear();
    staticObjects.clear();
    modelData.meshes.clear();
    tileMap.tileset.tiles.clear();

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

    modelData.load(fr);

    const auto numStaticObjects = fr.GetUInt32();
    ramsyscall_printf("num static objects: %d\n", numStaticObjects);
    staticObjects.resize(numStaticObjects);

    for (int i = 0; i < numStaticObjects; ++i) {
        auto& object = staticObjects[i];
        object.transform.translation.x.value = fr.GetInt32();
        object.transform.translation.y.value = fr.GetInt32();
        object.transform.translation.z.value = fr.GetInt32();

        std::uint16_t meshIndex = fr.GetInt16();
        object.mesh = modelData.meshes[meshIndex].makeInstance();

        // yaw (stored as 4.12, convert to 22.10)
        object.rotation.y.value = fr.GetInt16() >> 2;

        object.calculateWorldMatrix();
    }

    const auto numTiles = fr.GetUInt32();
    collisionBoxes.reserve(numTiles);
    ramsyscall_printf("num tiles: %d\n", numTiles);

    tileMap.tileset.tiles.resize(numTiles);
    for (int i = 0; i < numTiles; ++i) {
        auto flags = fr.GetInt8();
        auto id = fr.GetInt8();
        auto& ti = tileMap.tileset.tiles[id];
        if (flags & 0x1 != 0) { // model
            ti.modelId = fr.GetInt8();
        } else {
            ti.u0 = fr.GetInt8();
            ti.v0 = fr.GetInt8();
            ti.u1 = fr.GetInt8();
            ti.v1 = fr.GetInt8();
        }
        ti.height.value = fr.GetInt16();
    }

    const auto numColliders = fr.GetUInt32();
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

    const auto numTriggers = fr.GetUInt32();
    triggers.reserve(numTriggers);
    ramsyscall_printf("num triggers: %d\n", numTriggers);
    for (int i = 0; i < numTriggers; ++i) {
        Trigger trigger;

        trigger.name.value = fr.GetUInt32();

        trigger.aabb.min = psyqo::Vec3{
            .x = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
            .y = 0.0,
            .z = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
        };

        trigger.aabb.max = psyqo::Vec3{
            .x = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
            .y = 0.1,
            .z = psyqo::FixedPoint<>(fr.GetInt16(), psyqo::FixedPoint<>::RAW),
        };

        triggers.push_back(eastl::move(trigger));
    }
}
