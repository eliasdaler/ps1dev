#pragma once

#include <libgpu.h>

struct Camera {
    VECTOR position;
    SVECTOR rotation;
    MATRIX lookat;
};

namespace camera
{
void lookAt(Camera* camera, VECTOR* eye, VECTOR* target, VECTOR* up);
}
