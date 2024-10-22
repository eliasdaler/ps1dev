#pragma once

#include <psyqo/vector.hh>

struct Model;

struct Object {
    psyqo::Vec3 position{0.f, 0.f, 0.f};
    psyqo::Vector<2, 10> rotation; // pitch/yaw
    // VECTOR scale{ONE, ONE, ONE};
};

struct ModelObject : Object {
    Model* model{nullptr};
};
