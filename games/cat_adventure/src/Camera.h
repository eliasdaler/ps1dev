#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/vector.hh>

#include <Math/Transform.h>

struct CameraTransform {
    psyqo::Vec3 position{};
    psyqo::Vector<2, 10> rotation{};
};

struct Camera {
    psyqo::Vec3 position{};
    psyqo::Vector<2, 10> rotation{};

    void setTransform(const CameraTransform& transform)
    {
        position = transform.position;
        rotation = transform.rotation;
    }

    TransformMatrix view;
};
