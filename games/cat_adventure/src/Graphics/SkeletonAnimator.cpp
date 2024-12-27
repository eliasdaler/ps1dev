#include <Graphics/SkeletonAnimator.h>

#include <Graphics/Model.h>

#include <common/syscalls/syscalls.h>

void SkeletonAnimator::setAnimation(
    StringHash animationName,
    psyqo::FixedPoint<> playbackSpeed,
    psyqo::FixedPoint<> startAnimationPoint)
{
    if ((currentAnimation && animationName == currentAnimation->name) &&
        this->playbackSpeed == playbackSpeed) {
        return;
    }

    const auto anim = findAnimation(animationName);
    if (!anim) {
        ramsyscall_printf("Animation %s was not found\n", animationName.getStr());
        return;
    }

    currentAnimation = anim;
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

void SkeletonAnimator::animate(
    Armature& armature,
    eastl::vector<TransformMatrix>& jointGlobalTransforms) const
{
    if (armature.joints.empty()) {
        return;
    }

    if (!currentAnimation) {
        return;
    }

    const auto& animation = *currentAnimation;
    animateArmature(armature, animation, normalizedAnimTime);
    armature.calculateTransforms(jointGlobalTransforms);
}
