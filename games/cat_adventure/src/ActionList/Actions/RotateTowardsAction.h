#pragma once

#include <ActionList/Action.h>

#include <Core/StringHash.h>

#include <psyqo/fixed-point.hh>
#include <psyqo/trigonometry.hh>

struct AnimatedModelObject;

class RotateTowardsAction : public Action {
public:
    RotateTowardsAction(AnimatedModelObject& object,
        const AnimatedModelObject& target,
        psyqo::Angle rotationSpeed);

    bool enter() override;
    bool update(std::uint32_t dt) override;

private:
    AnimatedModelObject* objectPtr{nullptr};

    psyqo::Angle startAngle;
    psyqo::Angle endAngle;
    psyqo::Angle lerpFactor;
    psyqo::Angle lerpSpeed;
};
