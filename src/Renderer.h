#pragma once

#include <libgpu.h>

struct Object;
struct Mesh;
struct Model;
struct FastModelInstance;
struct Camera;

namespace renderer
{

struct RenderCtx {
    u_long* ot;
    int OTLEN;
    char* nextpri;
    const int currBuffer;
    RECT screenClip;
    MATRIX& worldmat;
    MATRIX& viewmat;
    Camera& camera;
};

// Returns new ctx.nextpri
[[nodiscard]] char* drawModel(
    RenderCtx& ctx,
    Object& object,
    const Model& model,
    TIM_IMAGE& texture,
    bool subdivide = false);

// Returns new ctx.nextpri
[[nodiscard]] char* drawMesh(
    RenderCtx& ctx,
    Object& object,
    const Mesh& mesh,
    const TIM_IMAGE& texture,
    bool subdivide);

// Returns new ctx.nextpri
[[nodiscard]] char* drawQuads(RenderCtx& ctx, const Mesh& mesh, u_long tpage, int clut);

// Returns new ctx.nextpri
char* drawModelFast(RenderCtx& ctx, Object& object, const FastModelInstance& mesh);

}
