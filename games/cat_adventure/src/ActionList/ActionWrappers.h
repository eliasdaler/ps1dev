#pragma once

#include <EASTL/unique_ptr.h>

#include <ActionList/Action.h>

#include <ActionList/Actions/WaitWhileAction.h>

class DialogueBox;

namespace actions
{
eastl::unique_ptr<Action> delay(std::uint32_t delayDurationSeconds);
eastl::unique_ptr<Action> waitWhile(WaitWhileAction::ConditionFuncType f);
eastl::unique_ptr<Action> say(DialogueBox& dialogueBox, eastl::string_view text);
}
