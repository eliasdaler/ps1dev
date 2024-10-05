#pragma once

#include <psyqo/vector.hh>

struct Model;

struct Object {
    psyqo::Vec3 position{0.f, 0.f, 0.f};
    psyqo::Vec3 rotation{0.f, 0.f, 0.f};
    // VECTOR scale{ONE, ONE, ONE};
};

struct ModelObject : Object {
    Model* model{nullptr};
};
