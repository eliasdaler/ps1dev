#include "RotateTowardsAction.h"

#include <Object.h>

RotateTowardsAction::RotateTowardsAction(AnimatedModelObject& object,
    const AnimatedModelObject& target,
    psyqo::FixedPoint<> rotationSpeed) :
    objectPtr(&object), targetPtr(&target), rotationSpeed(rotationSpeed)
{}

bool RotateTowardsAction::enter()
{
    objectPtr->rotateTowards(*targetPtr, rotationSpeed);
    return false;
}

bool RotateTowardsAction::update(std::uint32_t dt)
{
    return objectPtr->isRotating();
}
