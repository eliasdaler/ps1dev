#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/vector.hh>

struct Camera {
    psyqo::Vec3 position{};
    psyqo::Vector<2, 10> rotation{};

    psyqo::Matrix33 viewRot;
};
