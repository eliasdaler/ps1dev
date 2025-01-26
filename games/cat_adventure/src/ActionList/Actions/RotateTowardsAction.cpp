#include "RotateTowardsAction.h"

#include <Math/Math.h>
#include <Object.h>

RotateTowardsAction::RotateTowardsAction(AnimatedModelObject& object,
    const AnimatedModelObject& target,
    psyqo::Angle rotationSpeed) :
    objectPtr(&object), lerpSpeed(rotationSpeed)
{
    startAngle = object.getYaw();
    endAngle = object.findInteractionAngle(target);
    lerpFactor = 0.0;
    lerpSpeed = math::calculateLerpDelta(startAngle, endAngle, rotationSpeed);
}

bool RotateTowardsAction::enter()
{
    return false;
}

bool RotateTowardsAction::update(std::uint32_t dt)
{
    bool finished = false;

    lerpFactor += lerpSpeed;
    if (lerpFactor >= 1.0) { // finished rotation
        lerpSpeed = 1.0;
        finished = true;
    }

    objectPtr->setYaw(math::lerpAngle(startAngle, endAngle, lerpFactor));

    return finished;
}
