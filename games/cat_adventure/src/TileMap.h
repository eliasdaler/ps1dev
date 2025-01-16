#pragma once

#include <cstdint>

#include <EASTL/vector.h>

struct TileIndex {
    std::int16_t x, z;
};

struct TileInfo {
    std::uint8_t u0, v0; // top-left
    std::uint8_t u1, v1; // bottom-right
};

struct Tileset {
    eastl::vector<TileInfo> tiles;

    const TileInfo& getTileInfo(uint8_t tileId) const { return tiles[tileId]; }
};

struct Tile {
    uint8_t tileId;
    uint8_t modelId{0xFF}; //  only used if modelId != 0xFF
};

struct TileMap {
    // TODO: return ref
    Tile getTile(TileIndex ti) const;

    Tileset tileset;
};
