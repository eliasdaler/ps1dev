#pragma once

#include <Core/StringHash.h>

#include "Resources.h"

// TODO: automate this process - figure out how to not do this?
inline void initStringHashes()
{
    HASH_PUT("Walk");
    HASH_PUT("Run");
    HASH_PUT("Idle");
    HASH_PUT("Think");
    HASH_PUT("ThinkEnd");
    HASH_PUT("ThinkStart");

    // textures
    HASH_PUT2(CATO_TEXTURE_HASH);
    HASH_PUT2(BRICKS_TEXTURE_HASH);

    // models
    HASH_PUT2(HUMAN_MODEL_HASH);
    HASH_PUT2(CATO_MODEL_HASH);
    HASH_PUT2(CATO_FACES_TEXTURE_HASH);

    HASH_PUT2(LEVEL1_MODEL_HASH);
    HASH_PUT2(LEVEL2_MODEL_HASH);

    HASH_PUT2(LEVEL1_LEVEL_HASH);
}
