#pragma once

#include <cstddef>

#include <libgpu.h>

#include "Camera.h"

struct Object;
struct Mesh;
struct Model;
struct FastModelInstance;

struct Renderer {
    // data
    DISPENV dispEnv[2];
    DRAWENV drawEnv[2];

    static constexpr int OTLEN = 1 << 10;
    u_long ots[2][OTLEN];
    u_long* ot{nullptr}; // current frame OT

    static constexpr int PRIMBUFFLEN = 32768 * 4;
    std::byte primbuff[2][PRIMBUFFLEN];
    std::byte* nextpri{nullptr}; // pointer to primbuff
    short currBuffer{0};

    Camera camera;
    RECT screenClip;

    // currently drawn's object world/view matrices
    MATRIX worldmat;
    MATRIX viewmat;

    // functions
    void init();
    void beginDraw();
    void display();

    void drawModel(Object& object, const Model& model, TIM_IMAGE& texture, bool subdivide = false);
    void drawModelFast(Object& object, const FastModelInstance& mesh);

private:
    void drawMesh(Object& object, const Mesh& mesh, const TIM_IMAGE& texture, bool subdivide);
    void drawQuads(const Mesh& mesh, u_long tpage, int clut);
};
