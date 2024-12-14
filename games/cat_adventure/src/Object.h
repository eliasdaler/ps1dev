#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/trigonometry.hh>
#include <psyqo/vector.hh>

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

    psyqo::Vec3 position{0.f, 0.f, 0.f}; // world position
    psyqo::Vector<2, 10> rotation; // pitch/yaw
    // VECTOR scale{ONE, ONE, ONE};

    psyqo::Matrix33 worldMatrix; // M

    static psyqo::Trig<> trig;
};

struct ModelObject : Object {
    Model* model{nullptr};
};

struct MeshObject : Object {
    Mesh* mesh{nullptr};
};
