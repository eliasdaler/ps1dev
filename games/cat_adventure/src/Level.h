#pragma once

#include <EASTL/vector.h>

#include <Core/StringHash.h>

#include "Collision.h"
#include "Object.h"
#include "TileMap.h"

struct Level {
    int id{0};
    eastl::vector<StringHash> usedTextures;
    eastl::vector<StringHash> usedModels;

    eastl::vector<AABB> collisionBoxes;

    void load(const eastl::vector<uint8_t>& data);
    void loadNewFormat(const eastl::vector<uint8_t>& data);

    ModelData modelData;
    eastl::vector<MeshObject> staticObjects;

    TileMap tileMap;
};
