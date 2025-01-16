#include <TileMap.h>

Tile TileMap::getTile(TileIndex ti) const
{
    auto x = ti.x;
    auto z = ti.z;

    Tile info{
        .tileId = 3, // grass
    };

    if (z == 0) {
        info.tileId = 1; // road with stripe
    }

    if (z == -1 || z == -2 || z == 1 || z == 2) {
        info.tileId = 2; // road
    }

    if (z == -4 || z == 4) {
        info.tileId = 4; // curb
    }

    if (z == 3) { // curb transition
        info.tileId = 5;
        info.modelId = 4;
    }
    if (z == -3) { // curb transition
        info.tileId = 6;
        info.modelId = 5;
    }

    return info;
}
