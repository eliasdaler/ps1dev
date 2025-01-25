#include <ActionList/ActionWrappers.h>

#include <ActionList/Actions/DelayAction.h>
#include <ActionList/Actions/SayAction.h>

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

}
