#include "Object.h"

#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>

#include <psyqo/soft-math.hh>

#include "gte-math.h"

#include "Camera.h"

namespace
{
constexpr psyqo::Matrix33 I = psyqo::Matrix33{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
}

psyqo::Trig<> Object::trig{};

void Object::calculateWorldMatrix()
{
    if (rotation.x == 0.0 && rotation.y == 0.0) {
        worldMatrix = I;
        return;
    }

    // yaw
    worldMatrix =
        psyqo::SoftMath::generateRotationMatrix33(rotation.y, psyqo::SoftMath::Axis::Y, trig);

    if (rotation.x == 0.0) { // pitch
        return;
    }

    const auto rotX =
        psyqo::SoftMath::generateRotationMatrix33(rotation.x, psyqo::SoftMath::Axis::X, trig);
    // psyqo::SoftMath::multiplyMatrix33(objectRotMat, rotX, &objectRotMat);
    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(worldMatrix, rotX, &worldMatrix);
}
