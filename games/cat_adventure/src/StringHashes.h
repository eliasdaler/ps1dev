#pragma once

#include <Core/StringHash.h>

// TODO: automate this process - figure out how to not do this?
inline void initStringHashes()
{
    HASH_PUT("Walk");
    HASH_PUT("Run");
    HASH_PUT("Idle");
}
