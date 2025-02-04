#include "Transform.h"

#include <psyqo/soft-math.hh>

using namespace psyqo::GTE;
using namespace psyqo::GTE::Kernels;

TransformMatrix combineTransforms(const TransformMatrix& parentTransform, const Transform& local)
{
    TransformMatrix result;
    // R = R_parent * R_local
    psyqo::Matrix33 localRot = local.rotation.toRotationMatrix();

    psyqo::GTE::Math::multiplyMatrix33<PseudoRegister::Rotation, PseudoRegister::V0>(
        parentTransform.rotation, localRot, &result.rotation);

    // T = T_parent + R_parent * T_local
    psyqo::GTE::Math::matrixVecMul3<PseudoRegister::Rotation, PseudoRegister::V0>(
        local.translation, &result.translation);

    result.translation += parentTransform.translation;

    return result;
}

TransformMatrix combineTransforms(const TransformMatrix& parentTransform,
    const TransformMatrix& localTransform,
    bool gteMathForTranslation)
{
    TransformMatrix result;

    // R = R_parent * R_local
    psyqo::GTE::Math::multiplyMatrix33<PseudoRegister::Rotation, PseudoRegister::V0>(
        parentTransform.rotation, localTransform.rotation, &result.rotation);

    // T = T_parent + R_parent * T_local
    if (gteMathForTranslation) {
        psyqo::GTE::Math::matrixVecMul3<PseudoRegister::Rotation, // parentTransform.rotation is now
                                                                  // there
            PseudoRegister::V0>(localTransform.translation, &result.translation);
    } else {
        psyqo::SoftMath::matrixVecMul3(
            parentTransform.rotation, localTransform.translation, &result.translation);
    }

    result.translation += parentTransform.translation;

    return result;
}

void getRotationMatrix33RH(psyqo::Matrix33* m,
    psyqo::Angle t,
    psyqo::SoftMath::Axis a,
    const psyqo::Trig<>& trig)
{
    auto s = trig.sin(t);
    auto c = trig.cos(t);
    switch (a) {
    case psyqo::SoftMath::Axis::X:
        m->vs[0].x = 1.0;
        m->vs[0].y = 0.0;
        m->vs[0].z = 0.0;
        m->vs[1].x = 0.0;
        m->vs[1].y = c;
        m->vs[1].z = -s;
        m->vs[2].x = 0.0;
        m->vs[2].y = s;
        m->vs[2].z = c;
        break;
    case psyqo::SoftMath::Axis::Y:
        m->vs[0].x = c;
        m->vs[0].y = 0.0;
        m->vs[0].z = s;
        m->vs[1].x = 0.0;
        m->vs[1].y = 1.0;
        m->vs[1].z = 0.0;
        m->vs[2].x = -s;
        m->vs[2].y = 0.0;
        m->vs[2].z = c;
        break;
    case psyqo::SoftMath::Axis::Z:
        m->vs[0].x = c;
        m->vs[0].y = -s;
        m->vs[0].z = 0.0;
        m->vs[1].x = s;
        m->vs[1].y = c;
        m->vs[1].z = 0.0;
        m->vs[2].x = 0.0;
        m->vs[2].y = 0.0;
        m->vs[2].z = 1.0;
        break;
    }
}

void calculateViewMatrix(psyqo::Matrix33* m,
    psyqo::Angle pitch,
    psyqo::Angle yaw,
    const psyqo::Trig<>& trig)
{
    const auto sx = trig.sin(pitch);
    const auto cx = trig.cos(pitch);
    const auto sy = trig.sin(yaw);
    const auto cy = trig.cos(yaw);

    // We did the math
    m->vs[0].x = -cy;
    m->vs[0].y = 0.0;
    m->vs[0].z = sy;
    m->vs[1].x = -sx * sy;
    m->vs[1].y = -cx;
    m->vs[1].z = -sx * cy;
    m->vs[2].x = cx * sy;
    m->vs[2].y = -sx;
    m->vs[2].z = cx * cy;
}
