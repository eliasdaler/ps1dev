#include "SkeletonAnimator.h"

#include <common/syscalls/syscalls.h>

#include "Model.h"

void SkeletonAnimator::setAnimation(
    StringHash animationName,
    psyqo::FixedPoint<> playbackSpeed,
    psyqo::FixedPoint<> startAnimationPoint)
{
    if (animationName == currentAnimationName && this->playbackSpeed == playbackSpeed) {
        return;
    }

    const auto anim = findAnimation(animationName);
    if (!anim) {
        ramsyscall_printf("Animation %s was not found\n", animationName.getStr());
        return;
    }

    currentAnimation = anim;
    currentAnimationName = animationName;
    this->playbackSpeed = playbackSpeed;

    if (startAnimationPoint == 0.0) {
        if (playbackSpeed > 0.0) {
            normalizedAnimTime = 0.0;
        } else {
            normalizedAnimTime = 1.0;
        }
    } else {
        normalizedAnimTime = startAnimationPoint;
    }
    prevNormalizedAnimTime = normalizedAnimTime;
}

const SkeletalAnimation* SkeletonAnimator::findAnimation(StringHash animationName) const
{
    if (!animations) {
        return nullptr;
    }
    for (const auto& animation : *animations) {
        if (animation.name == animationName) {
            return &animation;
        }
    }
    return nullptr;
}

void SkeletonAnimator::update()
{
    prevNormalizedAnimTime = normalizedAnimTime;
    normalizedAnimTime += playbackSpeed;
    if (normalizedAnimTime > 1.0) { // loop
        normalizedAnimTime -= 1.0;
    }
    if (normalizedAnimTime < 0.0) {
        normalizedAnimTime = 1.0;
    }
}

int SkeletonAnimator::getAnimationFrame() const
{
    return (normalizedAnimTime * psyqo::FixedPoint<>(currentAnimation->length, 0)).integer();
}

bool SkeletonAnimator::frameJustChanged() const
{
    const auto prevFrame =
        (prevNormalizedAnimTime * psyqo::FixedPoint<>(currentAnimation->length, 0)).integer();
    return prevFrame != getAnimationFrame();
}

void SkeletonAnimator::animate(Model& model) const
{
    if (!currentAnimation) {
        return;
    }

    auto& armature = model.armature;
    const auto& animation = *currentAnimation;
    animateArmature(armature, animation, normalizedAnimTime);
    armature.calculateTransforms();
    armature.applySkinning(model.meshes[0]);
}
