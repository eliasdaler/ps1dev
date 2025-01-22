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

bitfield TriggerFlags {
  bool interaction : 1;
  reserved : 7;
};

struct Trigger {
    TriggerFlags flags;
    StringHash name;
    AABB aabb;
};

bitfield TileFlags {
  bool isModel : 1;
  reserved : 7;
};

struct TileInfo {
    TileFlags flags;
    u8 id;

    if (flags.isModel) {
        u8 modelId;
    } else {
        u8 u0, v0; // top-left
        u8 u1, v1; // bottom-right
    }

    FixedPoint<s16> height;
};

struct Level {
    u16 numUsedTextures;
    StringHash usedTextures[numUsedTextures];
    
    u16 numUsedModels;
    StringHash usedModels[numUsedModels];
    
    Model model;
    
    u32 numObjects;
    Object objects[numObjects];
    
    u32 numTiles;
    TileInfo tileset[numTiles];
    
    u32 numColliders;
    AABB colliders[numColliders];
    
    u32 numTriggers;
    Trigger triggers[numTriggers];
};

Level level @ 0x0;