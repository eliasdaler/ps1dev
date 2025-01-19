#pragma once

#include <cstdint>

#include <EASTL/vector.h>

#include <psyqo/fixed-point.hh>
#include <psyqo/vector.hh>

struct TileIndex {
    std::int16_t x, z;
};

struct TileInfo {
    std::uint8_t u0, v0; // top-left
    std::uint8_t u1, v1; // bottom-right
    psyqo::FixedPoint<12, std::int16_t> height{0.0};
};

struct Tileset {
    eastl::vector<TileInfo> tiles;

    const TileInfo& getTileInfo(uint8_t tileId) const { return tiles[tileId]; }
};

struct Tile {
    static constexpr int SIZE = 8;
    static constexpr psyqo::FixedPoint SCALE = 0.125; // 1 / SIZE

    static constexpr uint8_t NULL_TILE_ID = 0xFF;
    static constexpr uint8_t NULL_MODEL_ID = 0xFF;

    uint8_t tileId{NULL_TILE_ID};
    uint8_t modelId{NULL_MODEL_ID}; //  only used if modelId != 0xFF
};

struct TileMap {
    // TODO: return ref
    Tile getTile(TileIndex ti) const;

    static constexpr TileIndex getTileIndex(const psyqo::Vec3& pos)
    {
        return TileIndex{
            .x = (int16_t)(pos.x * Tile::SIZE).floor(),
            .z = (int16_t)(pos.z * Tile::SIZE).floor(),
        };
    }

    static constexpr psyqo::FixedPoint<> toWorldCoords(std::uint16_t idx)
    {
        // only for Tile::SIZE == 8!!!
        // 12 - 3 == 9 (same as doing psyqo::FixedPoint<>(idx, 0) * tileScale
        return psyqo::FixedPoint<>(idx << 9, psyqo::FixedPoint<>::RAW);
    }

    Tileset tileset;
};
