#include "SkeletalAnimation.h"

#include "Armature.h"

void animateArmature(
    Armature& armature,
    const SkeletalAnimation& animation,
    const psyqo::FixedPoint<>& normalizedAnimTime)
{
    // TODO: multiply by numFrames
    auto currentFrame = normalizedAnimTime * 100.f;

    for (const auto& track : animation.tracks) {
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
