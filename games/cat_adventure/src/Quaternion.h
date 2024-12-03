#pragma once

#include <cstdint>

#include <psyqo/fixed-point.hh>
#include <psyqo/matrix.hh>

struct Quaternion {
    psyqo::FixedPoint<12, std::int16_t> x, y, z, w;

    psyqo::Matrix33 toRotationMatrix() const;
    void normalize();
};

Quaternion operator*(const Quaternion& q1, const Quaternion& q2);

/* Works for small rotations only for now - okay for animation interpolation */
Quaternion slerp(
    const Quaternion& q1,
    const Quaternion& q2,
    const psyqo::FixedPoint<12, std::int16_t>& factor);
