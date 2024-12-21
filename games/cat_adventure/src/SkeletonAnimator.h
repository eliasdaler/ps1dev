#pragma once

#include <EASTL/vector.h>

#include "SkeletalAnimation.h"
#include "StringHash.h"

struct Model;

struct SkeletonAnimator {
    static constexpr psyqo::FixedPoint<> DEFAULT_PLAYBACK_SPEED{0.04};

    void setAnimation(
        StringHash animationName,
        psyqo::FixedPoint<> playbackSpeed = DEFAULT_PLAYBACK_SPEED,
        psyqo::FixedPoint<> startAnimationPoint = 0.0);

    const SkeletalAnimation* findAnimation(StringHash animationName) const;

    void update();
    void animate(Armature& armature, eastl::vector<TransformMatrix>& jointGlobalTransforms) const;

    int getAnimationFrame() const;
    bool frameJustChanged() const;

    // data
    eastl::vector<SkeletalAnimation>* animations{nullptr};

    const SkeletalAnimation* currentAnimation{nullptr};
    StringHash currentAnimationName;

    psyqo::FixedPoint<> normalizedAnimTime{0.0};

private:
    psyqo::FixedPoint<> prevNormalizedAnimTime{0.0};

    bool playAnimationForward{true};
    psyqo::FixedPoint<> playbackSpeed{0.04};
};
