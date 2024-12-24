#include "Math.h"

namespace math
{
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

    psyqo::Angle lerpSpeed = 0.0;
    if (diff.abs() < 0.001) {
        return 1.0;
    }
    return psyqo::FixedPoint<10>(0.04) / diff.abs();
}

}
