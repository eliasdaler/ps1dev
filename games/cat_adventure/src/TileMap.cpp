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

    if (z == -1 || z == -2 || z == -3 || z == -4 || z == 1 || z == 2 || z == 3 || z == 4) {
        info.tileId = 2; // road
    }

    if (z == -6 || z == 6) {
        info.tileId = 4; // curb
    }

    if (x == -1 || x == -2) {
        if (info.tileId == 3) {
            info.tileId = 4; // sidewalk (parallel)
        } else if (info.tileId == 1 || info.tileId == 2) {
            info.tileId = 0; // crossing
        }
    }

    if (z == 5) { // curb transition (near)
        info.tileId = 5;
        info.modelId = 4;
    }

    if (z == -5) { // curb transition (far)
        info.tileId = 6;
        info.modelId = 3;
    }

    return info;
}
