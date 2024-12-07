#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/vector.hh>

#include <psyqo/gte-registers.hh>

#include "Quaternion.h"

// we expect joint rotation matrix to be in L because
// R is used for rendering lines
inline constexpr auto JOINT_ROTATION_MATRIX_REGISTER{psyqo::GTE::PseudoRegister::Light};

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
