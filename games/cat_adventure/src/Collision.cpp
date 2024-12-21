#include "Collision.h"

bool circlesIntersect(const Circle& a, const Circle& b)
{
    const auto dx = a.center.x - b.center.x;
    const auto dz = a.center.z - b.center.z;
    const auto sqDist = dx * dx + dz * dz;

    const auto sqSumRad = (a.radius + b.radius) * (a.radius + b.radius);

    return sqDist <= sqSumRad;
}
