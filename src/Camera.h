#pragma once

#include <libgpu.h>

struct Camera {
    VECTOR position;
    SVECTOR rotation;
    MATRIX lookat;
};

namespace camera
{
void lookAt(Camera& camera, const VECTOR& eye, const VECTOR& target, const VECTOR& up);
}
