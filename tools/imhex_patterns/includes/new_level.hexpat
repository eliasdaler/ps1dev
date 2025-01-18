#include <math.hexpat>

import * from model as Model;

using StringHash = u32;

struct Object {
    FixedPoint<s32> x, y, z;
    u16 mesh;
    u16 pad [[hidden]];
};

struct Level {
    u16 numUsedTextures;
    StringHash usedTextures[numUsedTextures];
    u16 numUsedModels;
    StringHash usedModels[numUsedModels];
    Model model;
    u32 numObjects;
    Object objects[numObjects];
};

Level level @ 0x0;