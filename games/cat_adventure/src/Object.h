#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/trigonometry.hh>
#include <psyqo/vector.hh>

struct Model;
struct Mesh;

struct Object {
    void calculateWorldMatrix(); // trashes R

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
