#include "Transform.h"

#include <psyqo/soft-math.hh>

#include "gte-math.h"

#define USE_GTE_MATH

using namespace psyqo::GTE;
using namespace psyqo::GTE::Kernels;

psyqo::Vec3 TransformMatrix::transformPoint(const psyqo::Vec3& localPoint) const
{
    psyqo::Vec3 res = localPoint;
#ifdef USE_GTE_MATH
    // assume that rotation is in R
    psyqo::GTE::Math::matrixVecMul3<JOINT_ROTATION_MATRIX_REGISTER, PseudoRegister::V0>(res, &res);
#else
    psyqo::SoftMath::matrixVecMul3(rotation, res, &res);
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
    result.translation = local.translation;
#ifdef USE_GTE_MATH
    psyqo::GTE::Math::matrixVecMul3<
        PseudoRegister::Rotation,
        PseudoRegister::V0>(result.translation, &result.translation);
#else
    psyqo::SoftMath::
        matrixVecMul3(parentTransform.rotation, result.translation, &result.translation);
#endif

    result.translation += parentTransform.translation;

    return result;
}
