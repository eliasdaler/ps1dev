#include "Transform.h"

#include <psyqo/soft-math.hh>

psyqo::Vec3 TransformMatrix::transformPoint(const psyqo::Vec3& localPoint) const
{
    psyqo::Vec3 res = localPoint;
    psyqo::SoftMath::matrixVecMul3(rotation, res, &res);
    res += translation;
    return res;
}

TransformMatrix combineTransforms(const TransformMatrix& parentTransform, const Transform& local)
{
    TransformMatrix result;
    // R = R_parent * R_local
    psyqo::Matrix33 localRot = local.rotation.toRotationMatrix();
    psyqo::SoftMath::multiplyMatrix33(localRot, parentTransform.rotation, &result.rotation);

    // T = T_parent + R_parent * T_local
    result.translation = local.translation;
    psyqo::SoftMath::
        matrixVecMul3(parentTransform.rotation, result.translation, &result.translation);

    result.translation += parentTransform.translation;

    return result;
}
