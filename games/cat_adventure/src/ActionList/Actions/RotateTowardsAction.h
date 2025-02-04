#pragma once

#include <ActionList/Action.h>

#include <psyqo/fixed-point.hh>
#include <psyqo/trigonometry.hh>

struct AnimatedModelObject;

class RotateTowardsAction : public Action {
public:
    RotateTowardsAction(AnimatedModelObject& object,
        const AnimatedModelObject& target,
        psyqo::FixedPoint<> rotationSpeed);

    bool enter() override;
    bool isFinished() const override;

private:
    AnimatedModelObject* objectPtr{nullptr};
    const AnimatedModelObject* targetPtr{nullptr};
    psyqo::FixedPoint<> rotationSpeed;
};
