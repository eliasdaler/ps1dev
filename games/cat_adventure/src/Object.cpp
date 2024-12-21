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

void AnimatedModelObject::update()
{
    calculateWorldMatrix();
    animator.update();
    animator.animate(model->armature, jointGlobalTransforms);
}
