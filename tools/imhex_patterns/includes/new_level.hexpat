#include <math.hexpat>

import * from model as Model;

using StringHash = u32;

struct Object {
    FixedPoint<s32> x, y, z;
    u16 mesh;
    FixedPoint<s16> yaw;
};

struct AABB {
    Vec2 min;
    Vec2 max;
};

struct Level {
    u16 numUsedTextures;
    StringHash usedTextures[numUsedTextures];
    
    u16 numUsedModels;
    StringHash usedModels[numUsedModels];
    
    Model model;
    
    u32 numObjects;
    Object objects[numObjects];
    
    u32 numColliders;
    AABB colliders[numColliders];
};

Level level @ 0x0;