#pragma once

#include <cstdint>

struct TileIndex {
    std::int16_t x, z;
};

struct TileInfo {
    uint8_t tileId;
    uint8_t modelId{0xFF}; //  only used if modelId != 0xFF
};

struct TileMap {
    // TODO: return ref
    TileInfo getTile(TileIndex ti) const;
};
