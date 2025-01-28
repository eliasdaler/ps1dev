#include "Math.h"

#include <EASTL/fixed_string.h>
#include <common/syscalls/syscalls.h>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/soft-math.hh>
#include <psyqo/xprintf.h>

namespace math
{
psyqo::Vec3 normalize(const psyqo::Vec3& v)
{
    auto x = psyqo::FixedPoint<>(v.x);
    auto y = psyqo::FixedPoint<>(v.y);
    auto z = psyqo::FixedPoint<>(v.z);

    // sq = (x*x, y*y, z*z)
    psyqo::GTE::write<psyqo::GTE::Register::IR1, psyqo::GTE::Unsafe>(x.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR2, psyqo::GTE::Unsafe>(y.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR3, psyqo::GTE::Safe>(z.value);
    psyqo::GTE::Kernels::sqr();
    psyqo::Vec3 sq;
    psyqo::GTE::read<psyqo::GTE::PseudoRegister::LV>(sq);

    // s = x * x + y * y + z * z
    const auto s = sq.x + sq.y + sq.z;

    // r = 1 / sqrt(s)
    psyqo::GTE::write<psyqo::GTE::Register::LZCS, psyqo::GTE::Safe>(s.raw());
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

    return {x, y, z};
}

psyqo::Angle normalizeAngle(psyqo::Angle a)
{
    while (a > 1.0) {
        a -= 2.0;
    }
    while (a < -1.0) {
        a += 2.0;
    }
    return a;
}

psyqo::Angle lerpAngle(psyqo::Angle a, psyqo::Angle b, psyqo::Angle lerpFactor)
{
    if (lerpFactor == 0.0) {
        return a;
    } else if (lerpFactor == 1.0) {
        return b;
    }

    auto diff = b - a;
    if (diff > 1.0) {
        diff -= 2.0;
    } else if (diff < -1.0) {
        diff += 2.0;
    }

    return a + diff * lerpFactor;
}

psyqo::Angle calculateLerpDelta(psyqo::Angle a, psyqo::Angle b, psyqo::Angle speed)
{
    auto diff = b - a;
    if (diff > 1.0) {
        diff -= 2.0;
    } else if (diff < -1.0) {
        diff += 2.0;
    }

    if (diff.abs() < 0.001) {
        return 1.0;
    }
    return psyqo::FixedPoint<10>(0.04) / diff.abs();
}

// atan2 implementation
// Thanks to Siev for helping out with this and coming up with most of the code

#define POWER 6 // adjust to balance accuracy vs. LUT size

#define M_PI 3.14159265358979323846;

// angle_t is 0.16 fixed-point to get nice modulo behaviour.
// It goes from 0 (0) to 2pi (UINT16_MAX)
using angleFP16 = uint16_t;
static constexpr auto F_PI = (angleFP16)0x8000; // half-way point of u16 range

// Compile time atan2 - uses builtin_atan2 to achieve this - we don't want to use libm here
consteval angleFP16 compileTimeAtan2(int y, int x)
{
    return F_PI * __builtin_atan2((double)y, (double)x) / M_PI;
}

struct Atan2Lut {
    angleFP16 entries[(1 << POWER) + 1][1 << (POWER + 1)]{};

    consteval Atan2Lut()
    {
        // We don't need to store all (N1, N2) permutations from 0 to N
        // since atan2(y, x) == atan2(ay, ax) and we can compute
        // atan2(n_a, n_b) from atan2(n_b, n_a) if (n_a, n_b) is not
        // present in LUT, but (n_b, n_a) is
        for (int x = (1 << POWER); x <= (1 << (POWER + 1)); ++x) {
            for (int y = 1; y < x; ++y) {
                angleFP16 val = compileTimeAtan2(y, x);
                entries[x - (1 << POWER)][y - 1] = val;
            }
        }
    }
};

constexpr Atan2Lut atan2Lut;

// Takes 20.12 FP as inputs - psyqo::FixedPoint<> is not used for brevity
angleFP16 atan2impl(int32_t y, int32_t x)
{
    if (y < 0) return -atan2impl(-y, x);
    if (x < 0) return F_PI - atan2impl(y, -x);
    if (x < y) return F_PI / 2 - atan2impl(x, y);

    psyqo::GTE::write<psyqo::GTE::Register::LZCS, psyqo::GTE::Safe>(x);
    const auto zs = psyqo::GTE::readRaw<psyqo::GTE::Register::LZCR, psyqo::GTE::Safe>();

    int32_t shift = POWER - (32 - zs) + 1;
    if (shift >= 0) {
        x <<= shift;
        y <<= shift;
    } else {
        shift = -shift;
        x = ((x >> (shift - 1)) + 1) >> 1;
        y = ((y >> (shift - 1)) + 1) >> 1;
    }

    if (y == 0) return x < 0 ? F_PI : 0;
    if (x == 0) return F_PI / 2;
    if (x == y) return F_PI / 4;

    return (atan2Lut.entries[x - (1 << POWER)][y - 1]);
}

psyqo::Angle atan2(psyqo::FixedPoint<> y, psyqo::FixedPoint<> x)
{
    psyqo::Angle res;
    // from 0.16 to psyqo::Angle representation (where 1.0 is pi)
    res.value = atan2impl(y.value, x.value) >> 5;

    /* static eastl::fixed_string<char, 512> str;
    fsprintf(str, "x = %.2f y = %.2f, atan2(y, x) = %.4f", x, y, psyqo::FixedPoint<>(res));
    ramsyscall_printf("%s\n", str.c_str()); */

    return res;
}

psyqo::FixedPoint<> distanceSq(const psyqo::Vec3& a, const psyqo::Vec3& b)
{
    auto aMinusB = a - b;
    psyqo::GTE::write<psyqo::GTE::Register::IR1, psyqo::GTE::Unsafe>(aMinusB.x.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR2, psyqo::GTE::Unsafe>(aMinusB.y.value);
    psyqo::GTE::write<psyqo::GTE::Register::IR3, psyqo::GTE::Safe>(aMinusB.z.value);
    psyqo::GTE::Kernels::sqr();

    // Read (x*x, y*y, z*z) from LV
    psyqo::GTE::read<psyqo::GTE::PseudoRegister::LV>(aMinusB);
    return aMinusB.x + aMinusB.y + aMinusB.z;
}

}
