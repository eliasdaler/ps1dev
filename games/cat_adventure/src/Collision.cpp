#include "Collision.h"

#include <psyqo/soft-math.hh>

#include <EASTL/fixed_string.h>
#include <common/syscalls/syscalls.h>
#include <psyqo/xprintf.h>

bool circlesIntersect(const Circle& a, const Circle& b)
{
    const auto dx = a.center.x - b.center.x;
    const auto dz = a.center.z - b.center.z;
    const auto sqDist = dx * dx + dz * dz;

    const auto sqSumRad = (a.radius + b.radius) * (a.radius + b.radius);

    return sqDist <= sqSumRad;
}

psyqo::Vec2 getResolutionVector(const Circle& a, const Circle& b)
{
    const auto diff = a.center - b.center;
    const auto dist = psyqo::SoftMath::squareRoot(diff.x * diff.x + diff.z * diff.z);
    if (dist == 0) {
        return {};
    }
    const auto overlap = (a.radius + b.radius) - dist;

    psyqo::Vec2 res;
    res.x = (overlap * diff.x) / dist;
    res.y = (overlap * diff.z) / dist;
    return res;
}

namespace
{
psyqo::FixedPoint<> clamp(psyqo::FixedPoint<> v, psyqo::FixedPoint<> min, psyqo::FixedPoint<> max)
{
    v.value = eastl::clamp(v.value, min.value, max.value);
    return v;
}
}

bool circleAABBIntersect(const Circle& circle, const AABB& aabb)
{
    const auto closestX = clamp(circle.center.x, aabb.min.x, aabb.max.x);
    const auto closestZ = clamp(circle.center.z, aabb.min.z, aabb.max.z);

    const auto distSq = (closestX - circle.center.x) * (closestX - circle.center.x) +
                        (closestZ - circle.center.z) * (closestZ - circle.center.z);

    return distSq <= circle.radius * circle.radius;
}

psyqo::Vec2 getCollisionNormal(const Circle& circle, const AABB& wall)
{
    if (circle.center.x - circle.radius < wall.min.x) {
        return {-1.0f, 0.0f};
    } else if (circle.center.x + circle.radius > wall.max.x) {
        return {1.0f, 0.0f};
    } else if (circle.center.z - circle.radius < wall.min.z) {
        return {0.0f, -1.0f};
    } else if (circle.center.z + circle.radius > wall.max.z) {
        return {0.0f, 1.0f};
    }

    return {0.0f, 0.0f}; // No collision
}

psyqo::Vec2 getResolutionVector(const Circle& circle, const AABB& aabb)
{
    const auto closestX = clamp(circle.center.x, aabb.min.x, aabb.max.x);
    const auto closestZ = clamp(circle.center.z, aabb.min.z, aabb.max.z);

    const auto dx = closestX - circle.center.x;
    const auto dz = closestZ - circle.center.z;

    const auto dist = psyqo::SoftMath::squareRoot(dx * dx + dz * dz);
    const auto penetrationDepth = circle.radius - dist;

    psyqo::Vec2 res;

    // res.x = -penetrationDepth * dx / dist;
    // res.y = -penetrationDepth * dz / dist;

    auto normal = getCollisionNormal(circle, aabb);
    res.x = normal.x * penetrationDepth;
    res.y = normal.y * penetrationDepth;

    return res;
}
