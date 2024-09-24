#pragma once

#include <cstdint>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <libgpu.h>

struct Object;

struct Vertex {
    SVECTOR pos;
    CVECTOR uv;
    CVECTOR col;
};

struct Mesh {
    eastl::vector<Vertex> vertices;
    int numTris{0};
    int numQuads{0};
};

struct Model {
    eastl::vector<Mesh> meshes;
};

struct RenderCtx {
    u_long* ot;
    int OTLEN;
    char* nextpri;
    RECT screenClip;
};

Model loadModel(eastl::string_view filename);

[[nodiscard]] char* drawMesh(
    RenderCtx& ctx,
    Object& object,
    const Mesh& mesh,
    const TIM_IMAGE& texture,
    bool subdivide);

[[nodiscard]] char* drawQuads(RenderCtx& ctx, const Mesh& mesh, u_long tpage, int clut);
