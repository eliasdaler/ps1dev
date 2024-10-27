#pragma once

#include "gte-math.h"

#include "test_util.h"

#define USE_GTE_MATH

namespace testing
{
inline void testMatrix()
{
    using namespace testutil;

    // clang-format off
    const auto mI = psyqo::Matrix33{{
        {1.0, 0.0, 0.0}, 
        {0.0, 1.0, 0.0}, 
        {0.0, 0.0, 1.0},
    }};
    const auto mA = psyqo::Matrix33{{
        {0.75, 1.25, 0.25}, 
        {0.25, 0.5,  0.5}, 
        {0.5,  0.75, 0.75},
    }};
    const auto mB = psyqo::Matrix33{{
        {0.25, 0.5,  0.5}, 
        {0.25, 0.25, 0.25}, 
        {0.5,  0.25, 0.25},
    }};
    // mA * mB
    const auto expectedRes = psyqo::Matrix33{{
        {0.625,  0.75,  0.75},
        {0.4375, 0.375, 0.375},
        {0.6875, 0.625, 0.625},
    }};

    const auto vB = psyqo::Vec3{0.25, 0.5, 0.5};
    // mA * vB
    psyqo::Vec3 expectedVRes = psyqo::Vec3{0.9375, 0.5625, 0.875};
    // clang-format on

    using namespace psyqo::GTE;
    using namespace psyqo::GTE::Kernels;

    psyqo::Matrix33 res;
#ifdef USE_GTE_MATH
    psyqo::GTE::Math::multiplyMatrix33<PseudoRegister::Rotation, PseudoRegister::V0>(mA, mB, &res);
#else
    psyqo::SoftMath::multiplyMatrix33(mA, mB, &res);
#endif

    printMatrix("mA", mA);
    printMatrix("mB", mB);
    printMatrix("mA * mB", res);
    printSeparatorLine();

    assertMatrixEqual(expectedRes, res);

// check another overload
#ifdef USE_GTE_MATH
    psyqo::GTE::Math::multiplyMatrix33<PseudoRegister::Rotation, PseudoRegister::V0>(mB, &res);
    assertMatrixEqual(expectedRes, res);
#else
    res = psyqo::SoftMath::multiplyMatrix33(mA, mB);
#endif
    assertMatrixEqual(expectedRes, res);

    // mA * vB
    psyqo::Vec3 vRes;
#ifdef USE_GTE_MATH
    psyqo::GTE::Math::matrixVecMul3<PseudoRegister::Rotation, PseudoRegister::V0>(mA, vB, &vRes);
#else
    psyqo::SoftMath::matrixVecMul3(mA, vB, &vRes);
#endif
    assertVec3Equal(expectedVRes, vRes);

    printMatrix("mA", mA);
    printVec3("vB", vB);
    printVec3("mA * vB", vRes);
    printSeparatorLine();

#ifdef USE_GTE_MATH
    // check another overload
    psyqo::GTE::Math::matrixVecMul3<PseudoRegister::Rotation, PseudoRegister::V0>(vB, &vRes);
    assertVec3Equal(expectedVRes, vRes);
#endif

    // I * vB
#ifdef USE_GTE_MATH
    psyqo::GTE::Math::matrixVecMul3<PseudoRegister::Rotation, PseudoRegister::V0>(mI, vB, &vRes);
#else
    psyqo::SoftMath::matrixVecMul3(mI, vB, &vRes);
#endif
    expectedVRes = vB;
    assertVec3Equal(expectedVRes, vRes);

    printVec3("vB", vB);
    printVec3("I * vB", vRes);
    printSeparatorLine();
}
}
