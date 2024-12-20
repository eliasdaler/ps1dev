#include "Transform.h"

#include <psyqo/soft-math.hh>

#include "gte-math.h"

#define USE_GTE_MATH

using namespace psyqo::GTE;
using namespace psyqo::GTE::Kernels;

psyqo::Vec3 TransformMatrix::transformPoint(const psyqo::Vec3& localPoint) const
{
    psyqo::Vec3 res;
#ifdef USE_GTE_MATH
    // assume that rotation is in R
    psyqo::GTE::Math::
        matrixVecMul3<JOINT_ROTATION_MATRIX_REGISTER, PseudoRegister::V0>(localPoint, &res);
#else
    psyqo::SoftMath::matrixVecMul3(rotation, localPoint, &res);
#endif
    res += translation;
    return res;
}

TransformMatrix combineTransforms(const TransformMatrix& parentTransform, const Transform& local)
{
    TransformMatrix result;
    // R = R_parent * R_local
    psyqo::Matrix33 localRot = local.rotation.toRotationMatrix();

#ifdef USE_GTE_MATH
    psyqo::GTE::Math::multiplyMatrix33<
        PseudoRegister::Rotation,
        PseudoRegister::V0>(parentTransform.rotation, localRot, &result.rotation);
#else
    psyqo::SoftMath::multiplyMatrix33(localRot, parentTransform.rotation, &result.rotation);
#endif

    // T = T_parent + R_parent * T_local
#ifdef USE_GTE_MATH
    psyqo::GTE::Math::matrixVecMul3<
        PseudoRegister::Rotation,
        PseudoRegister::V0>(local.translation, &result.translation);
#else
    psyqo::SoftMath::
        matrixVecMul3(parentTransform.rotation, local.translation, &result.translation);
#endif

    result.translation += parentTransform.translation;

    return result;
}

TransformMatrix combineTransforms(
    const TransformMatrix& parentTransform,
    const TransformMatrix& localTransform)
{
    TransformMatrix result;

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(parentTransform.rotation);

    // R = R_parent * R_local
    psyqo::GTE::Math::multiplyMatrix33<
        PseudoRegister::Rotation,
        PseudoRegister::V0>(parentTransform.rotation, localTransform.rotation, &result.rotation);

    // T = T_parent + R_parent * T_local

    psyqo::GTE::Math::matrixVecMul3<
        PseudoRegister::Rotation,
        PseudoRegister::V0>(localTransform.translation, &result.translation);

    result.translation += parentTransform.translation;

    return result;
}

void getRotationMatrix33RH(
    psyqo::Matrix33* m,
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
