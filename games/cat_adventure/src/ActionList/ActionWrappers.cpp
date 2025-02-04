#include <ActionList/ActionWrappers.h>

#include <ActionList/Actions/CameraMoveAction.h>
#include <ActionList/Actions/DelayAction.h>
#include <ActionList/Actions/RotateTowardsAction.h>
#include <ActionList/Actions/SayAction.h>
#include <ActionList/Actions/SetAnimAndWaitAction.h>

#include <ActionList/ActionList.h>

#include <Object.h>

namespace actions
{

const ActionListBuilder& ActionListBuilder::parallelBegin() const
{
    actionList.enableParallelActionsMode();
    return *this;
}

const ActionListBuilder& ActionListBuilder::parallelEnd() const
{
    actionList.disableParallelActionsMode();
    return *this;
}

const ActionListBuilder& ActionListBuilder::delay(std::uint32_t delayDurationSeconds) const
{
    actionList.addAction(eastl::make_unique<DelayAction>(delayDurationSeconds));
    return *this;
}

const ActionListBuilder& ActionListBuilder::waitWhile(WaitWhileAction::ConditionFuncType f) const
{
    actionList.addAction(eastl::make_unique<WaitWhileAction>(eastl::move(f)));
    return *this;
}

const ActionListBuilder& ActionListBuilder::say(eastl::string_view text) const
{
    actionList.addAction(eastl::make_unique<SayAction>(dialogueBox, text));
    return *this;
}

const ActionListBuilder& ActionListBuilder::say(eastl::string_view text,
    const CameraTransform& transform) const
{
    actionList.addAction(eastl::make_unique<SayAction>(dialogueBox,
        text,
        SayParams{
            .cameraPtr = &camera,
            .cameraTransform = transform,
        }));
    return *this;
}

const ActionListBuilder& ActionListBuilder::say(eastl::string_view text,
    const SayParams& params) const
{
    actionList.addAction(eastl::make_unique<SayAction>(dialogueBox, text, params));
    return *this;
}
const ActionListBuilder& ActionListBuilder::say(eastl::string_view text,
    const CameraTransform& transform,
    AnimatedModelObject& object,
    StringHash animName) const
{
    return say(text,
        SayParams{
            .cameraPtr = &camera,
            .cameraTransform = transform,
            .object = &object,
            .anim = animName,
        });
}

const ActionListBuilder& ActionListBuilder::say(eastl::string_view text,
    AnimatedModelObject& object,
    StringHash animName) const
{
    return say(text,
        SayParams{
            .object = &object,
            .anim = animName,
        });
}

const ActionListBuilder& ActionListBuilder::say(eastl::string_view text,
    AnimatedModelObject& object,
    StringHash animName,
    StringHash faceAnimName) const
{
    return say(text,
        SayParams{
            .object = &object,
            .anim = animName,
            .faceAnim = faceAnimName,
        });
}

const ActionListBuilder& ActionListBuilder::doFunc(eastl::function<void()> f) const
{
    actionList.addAction(eastl::move(f));
    return *this;
}

const ActionListBuilder& ActionListBuilder::setCamera(const CameraTransform& transform) const
{
    actionList.addAction([camPtr = &camera, transform]() { camPtr->setTransform(transform); });
    return *this;
}

const ActionListBuilder& ActionListBuilder::moveCamera(const CameraTransform& transform,
    psyqo::FixedPoint<> moveTime) const
{
    actionList.addAction(eastl::make_unique<CameraMoveAction>(camera, transform, moveTime));
    return *this;
}

const ActionListBuilder& ActionListBuilder::lerp(LerpAction::LerpFuncType f,
    psyqo::FixedPoint<> time) const
{
    actionList.addAction(eastl::make_unique<LerpAction>(eastl::move(f), time));
    return *this;
}

const ActionListBuilder& ActionListBuilder::setAnim(AnimatedModelObject& object,
    StringHash animName) const
{
    actionList.addAction([&object, animName]() { object.animator.setAnimation(animName); });
    return *this;
}

const ActionListBuilder& ActionListBuilder::setAnim(AnimatedModelObject& object,
    StringHash animName,
    StringHash faceName) const
{
    actionList.addAction([&object, animName, faceName]() {
        object.animator.setAnimation(animName);
        object.setFaceAnimation(faceName);
    });
    return *this;
}

const ActionListBuilder& ActionListBuilder::setAnimAndWait(AnimatedModelObject& object,
    StringHash animName) const
{
    actionList.addAction(eastl::make_unique<SetAnimAndWaitAction>(object, animName));
    return *this;
}

const ActionListBuilder& ActionListBuilder::setAnimAndWait(AnimatedModelObject& object,
    StringHash animName,
    StringHash faceName) const
{
    actionList.addAction(eastl::make_unique<SetAnimAndWaitAction>(object, animName, faceName));
    return *this;
}

const ActionListBuilder& ActionListBuilder::setFaceAnim(AnimatedModelObject& object,
    StringHash faceName) const
{
    actionList.addAction([&object, faceName]() { object.setFaceAnimation(faceName); });
    return *this;
}

const ActionListBuilder& ActionListBuilder::rotateTowards(AnimatedModelObject& object,
    const AnimatedModelObject& target,
    psyqo::FixedPoint<> speed) const
{
    actionList.addAction(eastl::make_unique<RotateTowardsAction>(object, target, speed));
    return *this;
}

const ActionListBuilder& ActionListBuilder::rotateHeadTowards(AnimatedModelObject& object,
    const Quaternion& target,
    psyqo::FixedPoint<> time) const
{
    const auto start = object.model.armature.joints[4].localTransform.rotation;
    actionList.addAction(eastl::make_unique<LerpAction>(
        [&object, start, target](psyqo::FixedPoint<> lerpFactor) {
            object.manualHeadRotation = slerp(start, target, lerpFactor);
        },
        time));
    return *this;
}

const ActionListBuilder& ActionListBuilder::rotateHeadTowards(AnimatedModelObject& object,
    const Quaternion& start,
    const Quaternion& target,
    psyqo::FixedPoint<> time) const
{
    actionList.addAction(eastl::make_unique<LerpAction>(
        [&object, start, target](psyqo::FixedPoint<> lerpFactor) {
            object.manualHeadRotation = slerp(start, target, lerpFactor);
        },
        time));
    return *this;
}

}
