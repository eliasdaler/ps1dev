#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/vector.hh>

#include <psyqo/gte-registers.hh>
#include <psyqo/soft-math.hh>

#include "Quaternion.h"
#include "gte-math.h"

struct TransformMatrix {
    psyqo::Matrix33 rotation{};
    std::uint16_t pad;
    psyqo::Vec3 translation{};

    template<psyqo::GTE::PseudoRegister mreg, psyqo::GTE::PseudoRegister vreg>
    psyqo::Vec3 transformPoint(const psyqo::Vec3& localPoint) const
    {
        psyqo::Vec3 res;
        psyqo::GTE::Math::matrixVecMul3<mreg, vreg>(rotation, localPoint, &res);
        res += translation;
        return res;
    }

    /* Same as transformPoint, but rotation is already loaded into mreg */
    template<psyqo::GTE::PseudoRegister mreg, psyqo::GTE::PseudoRegister vreg>
    psyqo::Vec3 transformPointMatrixLoaded(const psyqo::Vec3& localPoint) const
    {
        psyqo::Vec3 res;
        psyqo::GTE::Math::matrixVecMul3<mreg, vreg>(localPoint, &res);
        res += translation;
        return res;
    }
};

struct Transform {
    psyqo::Vec3 translation{};
    std::uint16_t pad;
    Quaternion rotation{};
};

TransformMatrix combineTransforms(const TransformMatrix& parentTransform, const Transform& local);

/* Set gteMathForTranslation = false if one the results would be outside of 4.12 range */
TransformMatrix combineTransforms(
    const TransformMatrix& parentTransform,
    const TransformMatrix& localTransform,
    bool gteMathForTranslation = true);

void getRotationMatrix33RH(
    psyqo::Matrix33* m,
    psyqo::Angle t,
    psyqo::SoftMath::Axis a,
    const psyqo::Trig<>& trig);
