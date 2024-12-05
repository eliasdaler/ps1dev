#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/vector.hh>

#include "Quaternion.h"

struct TransformMatrix {
    psyqo::Matrix33 rotation{};
    std::uint16_t pad;
    psyqo::Vec3 translation{};

    psyqo::Vec3 transformPoint(const psyqo::Vec3& localPoint) const;
};

struct Transform {
    psyqo::Vec3 translation{};
    std::uint16_t pad;
    Quaternion rotation{};
};

TransformMatrix combineTransforms(const TransformMatrix& parentTransform, const Transform& local);
