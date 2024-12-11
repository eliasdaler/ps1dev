#pragma once

#include <EASTL/vector.h>

#include "SkeletalAnimation.h"
#include "StringHash.h"

struct Model;

struct SkeletonAnimator {
    void setAnimation(StringHash animationName);
    const SkeletalAnimation* findAnimation(StringHash animationName) const;

    void update();
    void animate(Model& model) const;

    // data
    eastl::vector<SkeletalAnimation>* animations{nullptr};

    const SkeletalAnimation* currentAnimation{nullptr};
    StringHash currentAnimationName;

    psyqo::FixedPoint<> normalizedAnimTime{0.0};

private:
};
