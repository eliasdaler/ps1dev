#pragma once

#include <libgpu.h>

#include "Camera.h"

struct Object;
struct Mesh;
struct Model;
struct FastModelInstance;

struct Renderer {
    // data
    u_long* ot;
    int OTLEN;
    char* nextpri;
    int currBuffer;

    RECT screenClip;
    MATRIX worldmat;
    MATRIX viewmat;
    Camera camera;

    // functions
    void drawModel(Object& object, const Model& model, TIM_IMAGE& texture, bool subdivide = false);
    void drawModelFast(Object& object, const FastModelInstance& mesh);

private:
    void drawMesh(Object& object, const Mesh& mesh, const TIM_IMAGE& texture, bool subdivide);
    void drawQuads(const Mesh& mesh, u_long tpage, int clut);
};
