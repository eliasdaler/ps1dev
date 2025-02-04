#pragma once

#include <EASTL/unique_ptr.h>

#include <Core/StringHash.h>

#include <ActionList/Action.h>

#include <ActionList/Actions/LerpAction.h>
#include <ActionList/Actions/SayAction.h>
#include <ActionList/Actions/WaitWhileAction.h>

class DialogueBox;
struct Camera;
struct CameraTransform;
struct ActionList;
struct AnimatedModelObject;

namespace actions
{
struct ActionListBuilder {
    ActionList& actionList;
    Camera& camera;
    DialogueBox& dialogueBox;

    const ActionListBuilder& parallelBegin() const;
    const ActionListBuilder& parallelEnd() const;

    const ActionListBuilder& delay(std::uint32_t delayDurationSeconds) const;
    const ActionListBuilder& waitWhile(WaitWhileAction::ConditionFuncType f) const;
    const ActionListBuilder& setCamera(const CameraTransform& transform) const;
    const ActionListBuilder& moveCamera(const CameraTransform& transform,
        psyqo::FixedPoint<> moveTime) const;

    const ActionListBuilder& lerp(LerpAction::LerpFuncType f, psyqo::FixedPoint<> time) const;

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

    const ActionListBuilder& rotateTowards(AnimatedModelObject& object,
        const AnimatedModelObject& target,
        psyqo::FixedPoint<> speed = 1.0) const;

    const ActionListBuilder& rotateHeadTowards(AnimatedModelObject& object,
        const Quaternion& target,
        psyqo::FixedPoint<> time) const;

    const ActionListBuilder& rotateHeadTowards(AnimatedModelObject& object,
        const Quaternion& start,
        const Quaternion& target,
        psyqo::FixedPoint<> time) const;
};

}
