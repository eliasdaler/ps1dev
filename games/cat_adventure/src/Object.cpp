#include "Object.h"

#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>

#include <psyqo/soft-math.hh>

#include "Camera.h"
#include "Model.h"
#include "Transform.h"
#include "gte-math.h"

namespace
{
constexpr psyqo::Matrix33 I = psyqo::Matrix33{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
}

psyqo::Trig<> Object::trig{};

void Object::calculateWorldMatrix()
{
    if (rotation.x == 0.0 && rotation.y == 0.0) {
        transform.rotation = I;
        return;
    }

    // yaw
    getRotationMatrix33RH(&transform.rotation, rotation.y, psyqo::SoftMath::Axis::Y, trig);

    if (rotation.x == 0.0) {
        return;
    }

    // pitch
    psyqo::Matrix33 rotX;
    getRotationMatrix33RH(&rotX, rotation.x, psyqo::SoftMath::Axis::X, trig);
    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(transform.rotation, rotX, &transform.rotation);
}

void AnimatedModelObject::updateCollision()
{
    collisionCircle.center = getPosition();
    collisionCircle.radius = 0.04;

    interactionCircle.center = getPosition() + getFront() * 0.05;
    interactionCircle.radius = 0.07;
}

void AnimatedModelObject::update()
{
    calculateWorldMatrix();
    animator.update();
    animator.animate(model->armature, jointGlobalTransforms);
}

psyqo::Angle AnimatedModelObject::findInteractionAngle(const Object& other)
{
    // FIXME: replace with atan2 later
    const auto oldAngle = rotation.y;
    static constexpr auto numAttemps = 60.0;
    psyqo::FixedPoint<> minDistSq = 1000.0;
    psyqo::Angle currAngle = 0.0;
    psyqo::Angle bestAngle = 0.0;
    psyqo::Angle incAngle = 2.0 / numAttemps;
    for (int i = 0; i < (int)numAttemps; ++i) {
        rotation.y = currAngle;
        auto fr = getFront();
        auto checkPoint = getPosition() + fr * 0.1;
        auto diff = checkPoint - other.getPosition();
        auto distSq = diff.x * diff.x + diff.z * diff.z;
        if (distSq < minDistSq) {
            bestAngle = currAngle;
            minDistSq = distSq;
        }
        currAngle += incAngle;
    }
    rotation.y = oldAngle;
    return bestAngle;
}
