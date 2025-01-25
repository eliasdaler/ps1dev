#pragma once

#include <EASTL/unique_ptr.h>

#include <Core/StringHash.h>

#include <ActionList/Action.h>

#include <ActionList/Actions/WaitWhileAction.h>

class DialogueBox;
class Camera;
struct CameraTransform;
struct ActionList;
struct AnimatedModelObject;

namespace actions
{
eastl::unique_ptr<Action> delay(std::uint32_t delayDurationSeconds);
eastl::unique_ptr<Action> waitWhile(WaitWhileAction::ConditionFuncType f);
eastl::unique_ptr<Action> say(DialogueBox& dialogueBox, eastl::string_view text);

struct ActionListBuilder {
    ActionList& actionList;
    Camera& camera;
    DialogueBox& dialogueBox;

    ActionListBuilder& delay(std::uint32_t delayDurationSeconds);
    ActionListBuilder& waitWhile(WaitWhileAction::ConditionFuncType f);
    ActionListBuilder& say(eastl::string_view text);
    ActionListBuilder& say(eastl::string_view text, const CameraTransform& transform);
    ActionListBuilder& doFunc(eastl::function<void()> f);
    ActionListBuilder& setAnim(AnimatedModelObject& object, StringHash animName);
    ActionListBuilder& setAnim(
        AnimatedModelObject& object,
        StringHash animName,
        StringHash faceName);
    ActionListBuilder& setAnimAndWait(AnimatedModelObject& object, StringHash animName);
    ActionListBuilder& setAnimAndWait(
        AnimatedModelObject& object,
        StringHash animName,
        StringHash faceName);
};

}
