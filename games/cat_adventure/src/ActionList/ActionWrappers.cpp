#include <ActionList/ActionWrappers.h>

#include <ActionList/Actions/DelayAction.h>
#include <ActionList/Actions/SayAction.h>

#include <ActionList/ActionList.h>

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
    actionList.addAction(eastl::make_unique<SayAction>(dialogueBox, text, camera, transform));
    return *this;
}

ActionListBuilder& ActionListBuilder::doFunc(eastl::function<void()> f)
{
    actionList.addAction(eastl::move(f));
    return *this;
}

}
