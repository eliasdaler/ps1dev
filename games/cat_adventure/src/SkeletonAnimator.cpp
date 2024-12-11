#include "SkeletonAnimator.h"

#include <common/syscalls/syscalls.h>

#include "Model.h"

void SkeletonAnimator::setAnimation(StringHash animationName)
{
    const auto anim = findAnimation(animationName);
    if (!anim) {
        ramsyscall_printf("Animation %s was not found\n", animationName.getStr());
        return;
    }

    currentAnimation = anim;
    currentAnimationName = animationName;

    normalizedAnimTime = 0.0;
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
    normalizedAnimTime += 0.04;
    if (normalizedAnimTime > 1.0) { // loop
        normalizedAnimTime -= 1.0;
    }
    if (normalizedAnimTime < 0.0) {
        normalizedAnimTime = 1.0;
    }
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
