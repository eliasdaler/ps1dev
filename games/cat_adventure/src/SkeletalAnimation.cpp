#include "SkeletalAnimation.h"

#include "Armature.h"
#include "FileReader.h"

#include <common/syscalls/syscalls.h>

void animateArmature(
    Armature& armature,
    const SkeletalAnimation& animation,
    const psyqo::FixedPoint<>& normalizedAnimTime)
{
    // TODO: multiply by numFrames
    auto currentFrame = normalizedAnimTime * 100.f;

    for (const auto& track : animation.tracks) {
        if (track.info == TRACK_TYPE_TRANSLATION) {
            continue;
        }
        auto& joint = armature.joints[track.joint];
        // TODO: use binary search instead
        int nextKeyIdx;
        for (nextKeyIdx = track.keys.size() - 1; nextKeyIdx >= 0; nextKeyIdx--) {
            if (currentFrame > track.keys[nextKeyIdx - 1].frame) {
                break;
            }
        }
        const auto& prevKey = track.keys[nextKeyIdx - 1];
        const auto& nextKey = track.keys[nextKeyIdx];

        // TODO: precompute 1 / (nextKey.frame - prevKey.frame) ?
        const auto lerpFactor = (nextKey.frame - currentFrame) / (nextKey.frame - prevKey.frame);
        // const auto lerpFactor = psyqo::FixedPoint<>(0.0);
        // TODO: for now only handle rotation keys
        joint.localTransform.rotation = slerp(
            prevKey.data.rotation,
            nextKey.data.rotation,
            psyqo::FixedPoint<12, std::int16_t>(lerpFactor));
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
