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

ActionListBuilder& ActionListBuilder::delay(std::uint32_t delayDurationSeconds)
{
    actionList.addAction(eastl::make_unique<DelayAction>(delayDurationSeconds));
    return *this;
}

ActionListBuilder& ActionListBuilder::waitWhile(WaitWhileAction::ConditionFuncType f)
{
    actionList.addAction(eastl::make_unique<WaitWhileAction>(eastl::move(f)));
    return *this;
}

ActionListBuilder& ActionListBuilder::say(eastl::string_view text)
{
    actionList.addAction(eastl::make_unique<SayAction>(dialogueBox, text));
    return *this;
}

ActionListBuilder& ActionListBuilder::say(eastl::string_view text, const CameraTransform& transform)
{
    actionList.addAction(eastl::make_unique<SayAction>(
        dialogueBox,
        text,
        SayParams{
            .cameraPtr = &camera,
            .cameraTransform = transform,
        }));
    return *this;
}

ActionListBuilder& ActionListBuilder::say(eastl::string_view text, const SayParams& params)
{
    actionList.addAction(eastl::make_unique<SayAction>(dialogueBox, text, params));
    return *this;
}
ActionListBuilder& ActionListBuilder::say(
    eastl::string_view text,
    const CameraTransform& transform,
    AnimatedModelObject& object,
    StringHash animName)
{
    return say(
        text,
        SayParams{
            .cameraPtr = &camera,
            .cameraTransform = transform,
            .object = &object,
            .anim = animName,
        });
}

ActionListBuilder& ActionListBuilder::say(
    eastl::string_view text,
    AnimatedModelObject& object,
    StringHash animName)
{
    return say(
        text,
        SayParams{
            .object = &object,
            .anim = animName,
        });
}

ActionListBuilder& ActionListBuilder::say(
    eastl::string_view text,
    AnimatedModelObject& object,
    StringHash animName,
    StringHash faceAnimName)
{
    return say(
        text,
        SayParams{
            .object = &object,
            .anim = animName,
            .faceAnim = faceAnimName,
        });
}

ActionListBuilder& ActionListBuilder::doFunc(eastl::function<void()> f)
{
    actionList.addAction(eastl::move(f));
    return *this;
}

ActionListBuilder& ActionListBuilder::setCamera(const CameraTransform& transform)
{
    actionList.addAction([camPtr = &camera, transform]() { camPtr->setTransform(transform); });
    return *this;
}

ActionListBuilder& ActionListBuilder::setAnim(AnimatedModelObject& object, StringHash animName)
{
    actionList.addAction([&object, animName]() { object.animator.setAnimation(animName); });
    return *this;
}

ActionListBuilder& ActionListBuilder::setAnim(
    AnimatedModelObject& object,
    StringHash animName,
    StringHash faceName)
{
    actionList.addAction([&object, animName, faceName]() {
        object.animator.setAnimation(animName);
        object.setFaceAnimation(faceName);
    });
    return *this;
}

ActionListBuilder& ActionListBuilder::setAnimAndWait(
    AnimatedModelObject& object,
    StringHash animName)
{
    actionList.addAction(eastl::make_unique<SetAnimAndWaitAction>(object, animName));
    return *this;
}

ActionListBuilder& ActionListBuilder::setAnimAndWait(
    AnimatedModelObject& object,
    StringHash animName,
    StringHash faceName)
{
    actionList.addAction(eastl::make_unique<SetAnimAndWaitAction>(object, animName, faceName));
    return *this;
}

ActionListBuilder& ActionListBuilder::setFaceAnim(AnimatedModelObject& object, StringHash faceName)
{
    actionList.addAction([&object, faceName]() { object.setFaceAnimation(faceName); });
    return *this;
}

}
