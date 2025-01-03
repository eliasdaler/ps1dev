#include "Quaternion.h"

#include <psyqo/soft-math.hh>

#include <EASTL/fixed_string.h>
#include <common/syscalls/syscalls.h>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/xprintf.h>

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
    psyqo::FixedPoint<> a,
    psyqo::FixedPoint<> b,
    psyqo::FixedPoint<> factor)
{
    return psyqo::FixedPoint<12, std::int16_t>(
        a * factor + (psyqo::FixedPoint<>(1.0) - factor) * b);
}

/* static eastl::fixed_string<char, 512> str;
fsprintf(str, "s = %.4f, 1/sqrt(s) = %.4f", s, r);
ramsyscall_printf("%s\n", str.c_str()); */

void Quaternion::normalize()
{
    auto x = psyqo::FixedPoint<>(this->x);
    auto y = psyqo::FixedPoint<>(this->y);
    auto z = psyqo::FixedPoint<>(this->z);
    auto w = psyqo::FixedPoint<>(this->w);

    // sq = (x*x, y*y, z*z)
    psyqo::GTE::write<psyqo::GTE::Register::IR1, psyqo::GTE::Unsafe>(x.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR2, psyqo::GTE::Unsafe>(y.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR3, psyqo::GTE::Safe>(z.value);
    psyqo::GTE::Kernels::sqr();
    psyqo::Vec3 sq;
    psyqo::GTE::read<psyqo::GTE::PseudoRegister::LV>(sq);

    // s = x * x + y * y + z * z + w * w
    const auto s = sq.x + sq.y + sq.z + w * w;

    // r = 1 / sqrt(s)
    psyqo::GTE::write<psyqo::GTE::Register::LZCS, psyqo::GTE::Unsafe>(s.raw());
    const auto approx = 1 << (psyqo::GTE::readRaw<psyqo::GTE::Register::LZCR>() - 9);
    const auto approxFP = psyqo::FixedPoint<>(approx, psyqo::FixedPoint<>::RAW);
    const auto r = psyqo::SoftMath::inverseSquareRoot(s, approxFP);

    /*
        x *= r;
        y *= r;
        z *= r;
    */
    psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(r.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR1, psyqo::GTE::Unsafe>(x.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR2, psyqo::GTE::Unsafe>(y.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR3, psyqo::GTE::Safe>(z.value);
    psyqo::GTE::Kernels::gpf();
    x.value = psyqo::GTE::readRaw<psyqo::GTE::Register::IR1, psyqo::GTE::Safe>();
    y.value = psyqo::GTE::readRaw<psyqo::GTE::Register::IR2, psyqo::GTE::Unsafe>();
    z.value = psyqo::GTE::readRaw<psyqo::GTE::Register::IR3, psyqo::GTE::Unsafe>();

    w *= r;

    this->x = psyqo::FixedPoint<12, std::int16_t>(x);
    this->y = psyqo::FixedPoint<12, std::int16_t>(y);
    this->z = psyqo::FixedPoint<12, std::int16_t>(z);
    this->w = psyqo::FixedPoint<12, std::int16_t>(w);
}

Quaternion slerp(const Quaternion& q1, const Quaternion& q2, psyqo::FixedPoint<> factor)
{
    Quaternion res;

    res.x = lerp(psyqo::FixedPoint<>(q1.x), psyqo::FixedPoint<>(q2.x), factor);
    res.y = lerp(psyqo::FixedPoint<>(q1.y), psyqo::FixedPoint<>(q2.y), factor);
    res.z = lerp(psyqo::FixedPoint<>(q1.z), psyqo::FixedPoint<>(q2.z), factor);
    res.w = lerp(psyqo::FixedPoint<>(q1.w), psyqo::FixedPoint<>(q2.w), factor);

    res.normalize();

    return res;
}
