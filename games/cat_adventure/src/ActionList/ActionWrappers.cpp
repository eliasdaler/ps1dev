#include <ActionList/ActionWrappers.h>

#include <ActionList/Actions/DelayAction.h>
#include <ActionList/Actions/SayAction.h>
#include <ActionList/Actions/SetAnimAndWaitAction.h>

#include <ActionList/ActionList.h>

#include <Object.h>

namespace actions
{

eastl::unique_ptr<Action> delay(std::uint32_t delayDurationSeconds)
{
    return eastl::make_unique<DelayAction>(delayDurationSeconds);
}

eastl::unique_ptr<Action> waitWhile(WaitWhileAction::ConditionFuncType f)
{
    return eastl::make_unique<WaitWhileAction>(eastl::move(f));
}

eastl::unique_ptr<Action> say(DialogueBox& dialogueBox, eastl::string_view text)
{
    return eastl::make_unique<SayAction>(dialogueBox, text);
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

}
