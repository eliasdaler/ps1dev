#pragma once

#include <EASTL/vector.h>

#include <Core/StringHash.h>
#include <Graphics/SkeletalAnimation.h>

struct Model;

struct SkeletonAnimator {
    static constexpr psyqo::FixedPoint<> DEFAULT_PLAYBACK_SPEED{1.0};

    void setAnimation(
        StringHash animationName,
        psyqo::FixedPoint<> playbackSpeed = DEFAULT_PLAYBACK_SPEED,
        psyqo::FixedPoint<> startAnimationPoint = 0.0);

    const SkeletalAnimation* findAnimation(StringHash animationName) const;

    void update();
    void animate(Armature& armature, eastl::vector<TransformMatrix>& jointGlobalTransforms) const;

    int getAnimationFrame() const;
    bool frameJustChanged() const;

    bool animationJustEnded() const { return !prevAnimationEnded && animationEnded; }
    bool hasAnimationEnded() const { return animationEnded; }

    StringHash getCurrentAnimationName() const
    {
        if (!currentAnimation) {
            return {};
        }
        return currentAnimation->name;
    }

    // data
    eastl::vector<SkeletalAnimation>* animations{nullptr};
    const SkeletalAnimation* currentAnimation{nullptr};
    psyqo::FixedPoint<> normalizedAnimTime{0.0};

private:
    psyqo::FixedPoint<> prevNormalizedAnimTime{0.0};

    psyqo::FixedPoint<> playbackSpeed{1.0};

    uint32_t currentTimeMcs{0};
    uint32_t animLengthMcs{0};

    // We calculate normalized time in ms as psyqo::FixedPoint<> doesn't have enough precision
    // to do it in microseconds
    psyqo::FixedPoint<> animLengthMs{0};

    bool animationEnded{false};
    bool prevAnimationEnded{false};
};
