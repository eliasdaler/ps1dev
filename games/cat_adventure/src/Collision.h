#pragma once

#include <psyqo/vector.hh>

struct Circle {
    psyqo::Vec3 center; // assume that center.y is always 0 for now
    psyqo::FixedPoint<> radius;
};

struct AABB {
    psyqo::Vec3 min;
    psyqo::Vec3 max;
};

bool circlesIntersect(const Circle& a, const Circle& b);

bool pointInAABB(const AABB& aabb, const psyqo::Vec3& p);

// returns XZ resolution vector
psyqo::Vec2 getResolutionVector(const Circle& a, const Circle& b);

bool circleAABBIntersect(const Circle& a, const AABB& b);

// returns XZ resolution vector
psyqo::Vec2 getResolutionVector(const Circle& circle, const AABB& aabb);
