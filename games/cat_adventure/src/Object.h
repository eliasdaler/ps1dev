#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/trigonometry.hh>
#include <psyqo/vector.hh>

#include "Transform.h"

struct Model;
struct Mesh;

struct Object {
    void calculateWorldMatrix(); // trashes R

    psyqo::Vec3 getFront(const psyqo::Trig<>& trig) const
    {
        // assume no yaw
        return {
            .x = trig.sin(rotation.y),
            .y = 0.0,
            .z = trig.cos(rotation.y),
        };
    }

    psyqo::Vec3 getRight(const psyqo::Trig<>& trig) const
    {
        // assume no yaw
        return {
            .x = trig.cos(rotation.y),
            .y = 0.0,
            .z = -trig.sin(rotation.y),
        };
    }

    void setPosition(
        const psyqo::FixedPoint<> x,
        const psyqo::FixedPoint<> y,
        const psyqo::FixedPoint<> z)
    {
        transform.translation.x = x;
        transform.translation.y = y;
        transform.translation.z = z;
    }
    void setPosition(const psyqo::Vec3& t) { transform.translation = t; }
    const psyqo::Vec3& getPosition() const { return transform.translation; }

    TransformMatrix transform; // M
    psyqo::Vector<2, 10> rotation; // rotation stored as pitch/yaw

    static psyqo::Trig<> trig;
};

struct ModelObject : Object {
    Model* model{nullptr};
};

struct MeshObject : Object {
    Mesh* mesh{nullptr};
};
