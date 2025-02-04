#include "CameraMoveAction.h"

#include <Math/Math.h>

CameraMoveAction::CameraMoveAction(Camera& camera,
    const CameraTransform& targetTransform,
    psyqo::FixedPoint<> time) :
    cameraPtr(&camera),
    end(targetTransform),
    moveDurationMicroseconds((time * 1000).integer() * 1000)
{}

bool CameraMoveAction::enter()
{
    currentTime = 0;
    start.position = cameraPtr->position;
    start.rotation = cameraPtr->rotation;
    return false;
}

void CameraMoveAction::update(std::uint32_t dt)
{
    currentTime += dt;

    auto lerpF = psyqo::FixedPoint<>(currentTime / 1000, 0) /
                 psyqo::FixedPoint<>(moveDurationMicroseconds / 1000, 0);
    if (lerpF > 1.0) {
        lerpF = 1.0;
    }

    cameraPtr->position.x = math::lerp(start.position.x, end.position.x, lerpF);
    cameraPtr->position.y = math::lerp(start.position.y, end.position.y, lerpF);
    cameraPtr->position.z = math::lerp(start.position.z, end.position.z, lerpF);

    cameraPtr->rotation.x =
        math::lerpAngle(start.rotation.x, end.rotation.x, psyqo::FixedPoint<10>(lerpF));
    cameraPtr->rotation.y =
        math::lerpAngle(start.rotation.y, end.rotation.y, psyqo::FixedPoint<10>(lerpF));
}

bool CameraMoveAction::isFinished() const
{
    return currentTime >= moveDurationMicroseconds;
}
