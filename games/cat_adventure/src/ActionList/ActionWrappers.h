#pragma once

#include <EASTL/unique_ptr.h>

#include <Core/StringHash.h>

#include <ActionList/Action.h>

#include <ActionList/Actions/SayAction.h>
#include <ActionList/Actions/WaitWhileAction.h>

class DialogueBox;
struct Camera;
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

    const ActionListBuilder& delay(std::uint32_t delayDurationSeconds) const;
    const ActionListBuilder& waitWhile(WaitWhileAction::ConditionFuncType f) const;
    const ActionListBuilder& setCamera(const CameraTransform& transform) const;

    const ActionListBuilder& say(eastl::string_view text) const;
    const ActionListBuilder& say(eastl::string_view text, const CameraTransform& transform) const;
    const ActionListBuilder& say(eastl::string_view text, const SayParams& params) const;

    // more overloads
    const ActionListBuilder& say(eastl::string_view text,
        const CameraTransform& transform,
        AnimatedModelObject& object,
        StringHash animName) const;
    const ActionListBuilder& say(eastl::string_view text,
        AnimatedModelObject& object,
        StringHash animName) const;
    const ActionListBuilder& say(eastl::string_view text,
        AnimatedModelObject& object,
        StringHash animName,
        StringHash faceAnimName) const;

    const ActionListBuilder& doFunc(eastl::function<void()> f) const;

    const ActionListBuilder& setAnim(AnimatedModelObject& object, StringHash animName) const;
    const ActionListBuilder& setAnim(AnimatedModelObject& object,
        StringHash animName,
        StringHash faceName) const;

    const ActionListBuilder& setAnimAndWait(AnimatedModelObject& object, StringHash animName) const;
    const ActionListBuilder& setAnimAndWait(AnimatedModelObject& object,
        StringHash animName,
        StringHash faceName) const;

    const ActionListBuilder& setFaceAnim(AnimatedModelObject& object, StringHash faceName) const;
};

}
