#include "SkeletalAnimation.h"

#include <EASTL/fixed_string.h>
#include <common/syscalls/syscalls.h>
#include <psyqo/xprintf.h>

#include <Core/FileReader.h>
#include <Graphics/Armature.h>

namespace
{
psyqo::FixedPoint<> lerp(psyqo::FixedPoint<> a, psyqo::FixedPoint<> b, psyqo::FixedPoint<> factor)
{
    return a * factor + (psyqo::FixedPoint<>(1.0) - factor) * b;
}

}

void animateArmature(
    Armature& armature,
    const SkeletalAnimation& animation,
    psyqo::FixedPoint<> normalizedAnimTime)
{
    const auto currentFrame = normalizedAnimTime * psyqo::FixedPoint<>(animation.length, 0);
    for (const auto& track : animation.tracks) {
        auto& joint = armature.joints[track.joint];

        if (track.keys.size() == 1) { // constant track.
                                      // TODO: don't even include it in this list?
            if (track.info == TRACK_TYPE_ROTATION) {
                joint.localTransform.rotation = track.keys[0].data.rotation;
                continue;
            } else {
                const auto& tr = track.keys[0].data.translation;
                joint.localTransform.translation.x = psyqo::FixedPoint<>(tr.x);
                joint.localTransform.translation.y = psyqo::FixedPoint<>(tr.y);
                joint.localTransform.translation.z = psyqo::FixedPoint<>(tr.z);
                continue;
            }
        }

        // TODO: use binary search instead
        int nextKeyIdx;
        for (nextKeyIdx = track.keys.size() - 1; nextKeyIdx >= 0; nextKeyIdx--) {
            if (currentFrame >= track.keys[nextKeyIdx - 1].frame) {
                break;
            }
        }
        if (nextKeyIdx == -1) {
            nextKeyIdx = 1;
        }
        const auto& prevKey = track.keys[nextKeyIdx - 1];
        const auto& nextKey = track.keys[nextKeyIdx];

#define DO_LERP

#ifdef DO_LERP
        // TODO: precompute 1 / (nextKey.frame - prevKey.frame) ?
        auto lerpFactor = (nextKey.frame - currentFrame) / (nextKey.frame - prevKey.frame);
#endif

        if (track.info == TRACK_TYPE_ROTATION) {
#ifndef DO_LERP
            joint.localTransform.rotation = prevKey.data.rotation;
#else
            joint.localTransform.rotation =
                slerp(prevKey.data.rotation, nextKey.data.rotation, lerpFactor);
#endif
        } else if (track.info == TRACK_TYPE_TRANSLATION) {
#ifndef DO_LERP
            const auto& tr = prevKey.data.translation;
            joint.localTransform.translation.x = psyqo::FixedPoint<>(tr.x);
            joint.localTransform.translation.y = psyqo::FixedPoint<>(tr.y);
            joint.localTransform.translation.z = psyqo::FixedPoint<>(tr.z);
#else
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
#endif
        }
    }
}

void loadAnimations(
    const eastl::vector<uint8_t>& data,
    eastl::vector<SkeletalAnimation>& animations)
{
    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numAnimations = fr.GetUInt32();
    for (int j = 0; j < numAnimations; ++j) {
        SkeletalAnimation animation;
        animation.name.value = fr.GetUInt32();
        animation.flags = fr.GetUInt32();
        animation.length = fr.GetUInt16();
        animation.numTracks = fr.GetUInt16();
        animation.tracks.reserve(animation.numTracks);
        for (int i = 0; i < animation.numTracks; ++i) {
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
            animation.tracks.push_back(eastl::move(track));
        }
        animations.push_back(eastl::move(animation));
    }
}
