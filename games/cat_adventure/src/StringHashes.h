#pragma once

#include <Core/StringHash.h>

// TODO: automate this process - figure out how to not do this?
inline void initStringHashes()
{
    HASH_PUT("Walk");
    HASH_PUT("Run");
    HASH_PUT("Idle");

    // textures
    HASH_PUT("CATO.TIM;1");
    HASH_PUT("CAR.TIM;1");
    HASH_PUT("BRICKS.TIM;1");

    // models
    HASH_PUT("HUMAN.BIN;1");
    HASH_PUT("CATO.BIN;1");
    HASH_PUT("CAR.BIN;1");

    HASH_PUT("LEVEL.BIN;1");
    HASH_PUT("LEVEL2.BIN;1");
}
