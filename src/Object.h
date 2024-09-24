#pragma once

#include <libgpu.h>
struct Object {
    SVECTOR rotation{};
    VECTOR position{};
    VECTOR scale{ONE, ONE, ONE};
};
