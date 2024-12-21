#pragma once

#include <psyqo/vector.hh>

struct Circle {
    psyqo::Vec3 center; // assume that center.y is always 0 for now
    psyqo::FixedPoint<> radius;
};

bool circlesIntersect(const Circle& a, const Circle& b);
