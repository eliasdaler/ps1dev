#pragma once

#include <EASTL/vector.h>

#include <Core/StringHash.h>

struct Level {
    int id{0};
    eastl::vector<StringHash> usedTextures;
};
