#pragma once

#include <cstdint>

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <psyqo/fixed-point.hh>
#include <psyqo/vector.hh>

#include "Quaternion.h"
#include "StringHash.h"

struct Armature;

struct AnimationKey {
    psyqo::FixedPoint<> frame;
    union
    {
        psyqo::Vector<4, 12, std::int16_t> translation;
        Quaternion rotation;
    } data;
};

static constexpr uint8_t TRACK_TYPE_ROTATION = 0;
static constexpr uint8_t TRACK_TYPE_TRANSLATION = 1;
static constexpr uint8_t TRACK_TYPE_SCALE = 2;

struct AnimationTrack {
    std::uint8_t info; // first two bytes - type (00 - rot, 01 - trans, 10 - scale)
    std::uint8_t joint;
    std::uint16_t _pad;
    eastl::vector<AnimationKey> keys;
};

struct SkeletalAnimation {
    StringHash name;
    std::uint8_t numTracks;
    std::uint8_t numConstTracks;
    std::uint16_t length;
    eastl::vector<AnimationTrack> tracks;
};

void loadAnimations(
    const eastl::vector<uint8_t>& data,
    eastl::vector<SkeletalAnimation>& animations);

void animateArmature(
    Armature& armature,
    const SkeletalAnimation& animation,
    const psyqo::FixedPoint<>& normalizedAnimTime);
