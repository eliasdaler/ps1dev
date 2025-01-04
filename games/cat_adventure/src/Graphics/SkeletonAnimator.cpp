#include <Graphics/SkeletonAnimator.h>

#include <Graphics/Model.h>

#include <common/syscalls/syscalls.h>

#include <Game.h>

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

    currentTimeMcs = 0;
    animationEnded = false;
    prevNormalizedAnimTime = normalizedAnimTime;
    prevAnimationEnded = false;

    animLengthMcs = currentAnimation->length * (1'000'000 / 30); // anim is at 30 FPS
    animLengthMs = psyqo::FixedPoint<>((animLengthMcs / 1000), 0);
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
    if (!currentAnimation || animationEnded) {
        return;
    }

    if (playbackSpeed == 1.0) {
        currentTimeMcs += g_game.frameDtMcs;
    } else {
        currentTimeMcs +=
            (psyqo::FixedPoint<>(g_game.frameDtMcs, 0) * playbackSpeed.abs()).integer();
    }

    prevNormalizedAnimTime = normalizedAnimTime;
    prevAnimationEnded = animationEnded;

    if (!currentAnimation->isLooped() && currentTimeMcs > animLengthMcs) {
        if (playbackSpeed < 0.0) {
            normalizedAnimTime = 0.0;
            animationEnded = true;
            return;
        } else {
            normalizedAnimTime = 1.0;
            animationEnded = true;
            return;
        }
    }

    if (currentTimeMcs > animLengthMcs) {
        currentTimeMcs -= animLengthMcs;
    }

    normalizedAnimTime = psyqo::FixedPoint<>(currentTimeMcs / 1000, 0) / animLengthMs;
    if (playbackSpeed < 0.0) {
        normalizedAnimTime = psyqo::FixedPoint<>(1.0) - normalizedAnimTime;
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
