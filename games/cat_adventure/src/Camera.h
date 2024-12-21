#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/vector.hh>

#include "Transform.h"

struct Camera {
    psyqo::Vec3 position{};
    psyqo::Vector<2, 10> rotation{};

    TransformMatrix view;
};
