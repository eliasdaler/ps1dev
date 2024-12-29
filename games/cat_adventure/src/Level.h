#pragma once

#include <EASTL/vector.h>

#include <Core/StringHash.h>

#include "Collision.h"

struct Level {
    int id{0};
    eastl::vector<StringHash> usedTextures;
    eastl::vector<StringHash> usedModels;

    eastl::vector<AABB> collisionBoxes;

    void load(const eastl::vector<uint8_t>& data);
};
