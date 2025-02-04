#pragma once

#include <ActionList/Action.h>

#include <Core/StringHash.h>

struct AnimatedModelObject;

class SetAnimAndWaitAction : public Action {
public:
    SetAnimAndWaitAction(AnimatedModelObject& object, StringHash animName);
    SetAnimAndWaitAction(AnimatedModelObject& object, StringHash animName, StringHash faceAnimName);

    bool enter() override;
    bool isFinished() const override;

private:
    AnimatedModelObject* objectPtr{nullptr};
    StringHash animName{};
    StringHash faceAnimName{};
};
