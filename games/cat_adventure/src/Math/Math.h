#pragma once

#include <psyqo/fixed-point.hh>
#include <psyqo/trigonometry.hh>

namespace math
{
psyqo::Angle lerpAngle(psyqo::Angle a, psyqo::Angle b, psyqo::Angle lerpFactor);
psyqo::Angle calculateLerpDelta(psyqo::Angle a, psyqo::Angle b, psyqo::Angle speed);

psyqo::Angle atan2(psyqo::FixedPoint<> y, psyqo::FixedPoint<> x);
}
