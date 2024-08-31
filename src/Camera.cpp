#include "Camera.h"

namespace
{
VECTOR VectorCross(const VECTOR& a, const VECTOR& b)
{
    VECTOR out;
    OuterProduct12(const_cast<VECTOR*>(&a), const_cast<VECTOR*>(&b), &out);
    return out;
}
}

namespace camera
{
void lookAt(Camera& camera, const VECTOR& eye, const VECTOR& target, const VECTOR& up)
{
    VECTOR x, y, z;

    VECTOR zforward;
    zforward.vx = target.vx - eye.vx;
    zforward.vy = target.vy - eye.vy;
    zforward.vz = target.vz - eye.vz;
    VectorNormal(&zforward, &z);

    auto xright = VectorCross(z, up);
    VectorNormal(&xright, &x);

    auto yup = VectorCross(z, x);
    VectorNormal(&yup, &y);

    camera.lookat.m[0][0] = x.vx;
    camera.lookat.m[0][1] = x.vy;
    camera.lookat.m[0][2] = x.vz;

    camera.lookat.m[1][0] = y.vx;
    camera.lookat.m[1][1] = y.vy;
    camera.lookat.m[1][2] = y.vz;

    camera.lookat.m[2][0] = z.vx;
    camera.lookat.m[2][1] = z.vy;
    camera.lookat.m[2][2] = z.vz;

    VECTOR pos{-eye.vx, -eye.vy, -eye.vz};
    VECTOR t;
    ApplyMatrixLV(&camera.lookat, &pos, &t);
    TransMatrix(&camera.lookat, &t);
}
}
