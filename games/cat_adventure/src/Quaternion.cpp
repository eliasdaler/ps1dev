#include "Quaternion.h"

#include <psyqo/soft-math.hh>

psyqo::Matrix33 Quaternion::toRotationMatrix() const
{
    const auto x2 = x * x;
    const auto y2 = y * y;
    const auto z2 = z * z;
    const auto xy = x * y;
    const auto xz = x * z;
    const auto yz = y * z;
    const auto wx = w * x;
    const auto wy = w * y;
    const auto wz = w * z;
    static constexpr auto one = psyqo::FixedPoint<12, std::int16_t>(1.0);
    static constexpr auto two = psyqo::FixedPoint<12, std::int16_t>(2.0);

    using FP = psyqo::FixedPoint<>;
    return psyqo::Matrix33{{
        {FP{one - two * (y2 + z2)}, FP{two * (xy - wz)}, FP{two * (xz + wy)}},
        {FP{two * (xy + wz)}, FP{one - two * (x2 + z2)}, FP{two * (yz - wx)}},
        {FP{two * (xz - wy)}, FP{two * (yz + wx)}, FP{one - two * (x2 + y2)}},
    }};
}

Quaternion operator*(const Quaternion& q1, const Quaternion& q2)
{
    return Quaternion{
        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w,
        q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.w,
    };
}

psyqo::FixedPoint<12, std::int16_t> lerp(
    const psyqo::FixedPoint<12, std::int16_t>& a,
    const psyqo::FixedPoint<12, std::int16_t>& b,
    const psyqo::FixedPoint<12, std::int16_t>& factor)
{
    return a + factor * (b - a);
}

void Quaternion::normalize()
{
    auto x = this->x;
    auto y = this->y;
    auto z = this->z;
    auto w = this->w;
    const auto s = x * x + y * y + z * z + w * w;
    const auto r = psyqo::FixedPoint<12, std::int16_t>(
        psyqo::SoftMath::inverseSquareRoot((psyqo::FixedPoint<>{s})));
    x *= r;
    y *= r;
    z *= r;
    w *= r;
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
}

Quaternion slerp(
    const Quaternion& q1,
    const Quaternion& q2,
    const psyqo::FixedPoint<12, std::int16_t>& factor)
{
    Quaternion res;
    res.x = lerp(q1.x, q2.x, factor);
    res.y = lerp(q1.y, q2.y, factor);
    res.z = lerp(q1.z, q2.z, factor);
    res.w = lerp(q1.w, q2.w, factor);

    res.normalize();

    return res;
}
