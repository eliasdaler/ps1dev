#include "SkeletalAnimation.h"

#include "Armature.h"
#include "FileReader.h"

#include <common/syscalls/syscalls.h>

namespace
{
psyqo::FixedPoint<> lerp(
    const psyqo::FixedPoint<>& a,
    const psyqo::FixedPoint<>& b,
    const psyqo::FixedPoint<>& factor)
{
    return a + factor * (b - a);
}

}

void animateArmature(
    Armature& armature,
    const SkeletalAnimation& animation,
    const psyqo::FixedPoint<>& normalizedAnimTime)
{
    // TODO: multiply by numFrames
    // auto currentFrame = normalizedAnimTime * 31.f;
    auto currentFrame = normalizedAnimTime;

    for (const auto& track : animation.tracks) {
        auto& joint = armature.joints[track.joint];
        // TODO: use binary search instead
        int nextKeyIdx;
        for (nextKeyIdx = track.keys.size() - 1; nextKeyIdx >= 0; nextKeyIdx--) {
            if (currentFrame > track.keys[nextKeyIdx - 1].frame) {
                break;
            }
        }
        bool atStart = false;
        if (nextKeyIdx == -1) {
            atStart = true;
            nextKeyIdx = 1;
        }
        const auto& prevKey = track.keys[nextKeyIdx - 1];
        const auto& nextKey = track.keys[nextKeyIdx];

        // TODO: precompute 1 / (nextKey.frame - prevKey.frame) ?
        auto lerpFactor = (nextKey.frame - currentFrame) / (nextKey.frame - prevKey.frame);
        if (atStart) {
            lerpFactor = 0.0;
        }
        if (track.info == TRACK_TYPE_ROTATION) {
            joint.localTransform.rotation = slerp(
                prevKey.data.rotation,
                nextKey.data.rotation,
                psyqo::FixedPoint<12, std::int16_t>(lerpFactor));
        } else if (track.info == TRACK_TYPE_TRANSLATION) {
            joint.localTransform.translation.x = lerp(
                psyqo::FixedPoint<>(prevKey.data.translation.x),
                psyqo::FixedPoint<>(nextKey.data.translation.x),
                lerpFactor);
            joint.localTransform.translation.y = lerp(
                psyqo::FixedPoint<>(prevKey.data.translation.y),
                psyqo::FixedPoint<>(nextKey.data.translation.y),
                lerpFactor);
            joint.localTransform.translation.z = lerp(
                psyqo::FixedPoint<>(prevKey.data.translation.z),
                psyqo::FixedPoint<>(nextKey.data.translation.z),
                lerpFactor);
        }
    }
}

void SkeletalAnimation::load(const eastl::vector<uint8_t>& data)
{
    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numAnimations = fr.GetUInt32();
    numTracks = fr.GetUInt32();
    tracks.reserve(numTracks);
    for (int i = 0; i < numTracks; ++i) {
        AnimationTrack track;
        track.info = fr.GetUInt8();
        track.joint = fr.GetUInt8();
        const auto numKeys = fr.GetUInt16();
        track.keys.reserve(numKeys);
        for (int j = 0; j < numKeys; ++j) {
            AnimationKey key{};
            key.frame.value = fr.GetInt32();
            if (track.info == TRACK_TYPE_ROTATION) {
                key.data.rotation.w.value = fr.GetInt16();
                key.data.rotation.x.value = fr.GetInt16();
                key.data.rotation.y.value = fr.GetInt16();
                key.data.rotation.z.value = fr.GetInt16();
            } else if (track.info == TRACK_TYPE_TRANSLATION) {
                key.data.translation.x.value = fr.GetInt16();
                key.data.translation.y.value = fr.GetInt16();
                key.data.translation.z.value = fr.GetInt16();
                fr.SkipBytes(2);
            }
            track.keys.push_back(eastl::move(key));
        }
        tracks.push_back(eastl::move(track));
    }
}
