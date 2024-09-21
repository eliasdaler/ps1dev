#pragma once

#include <libgpu.h>

struct Camera {
    VECTOR position{};
    VECTOR rotation{};
    MATRIX view{};
};

namespace camera
{
void lookAt(Camera& camera, const VECTOR& eye, const VECTOR& target, const VECTOR& up);
}
