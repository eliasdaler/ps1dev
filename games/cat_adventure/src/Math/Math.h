#pragma once

#include <psyqo/fixed-point.hh>
#include <psyqo/trigonometry.hh>
#include <psyqo/vector.hh>

namespace math
{
psyqo::Vec3 normalize(const psyqo::Vec3& v);
psyqo::Angle normalizeAngle(psyqo::Angle a);
psyqo::Angle lerpAngle(psyqo::Angle a, psyqo::Angle b, psyqo::Angle lerpFactor);
psyqo::Angle calculateLerpDelta(psyqo::Angle a, psyqo::Angle b, psyqo::Angle speed);

psyqo::FixedPoint<> distanceSq(const psyqo::Vec3& a, const psyqo::Vec3& b);

psyqo::Angle atan2(psyqo::FixedPoint<> y, psyqo::FixedPoint<> x);

// TODO: gte lerp
inline psyqo::FixedPoint<> lerp(psyqo::FixedPoint<> a,
    psyqo::FixedPoint<> b,
    psyqo::FixedPoint<> factor)
{
    return a + (b - a) * factor;
}

}
