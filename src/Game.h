#pragma once

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include <cstdint>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/span.h>
#include <EASTL/unique_ptr.h>

#include "Camera.h"
#include "FastModel.h"
#include "Model.h"

inline constexpr int SCREENXRES = 320;
inline constexpr int SCREENYRES = 240;

struct Object {
    SVECTOR rotation{};
    VECTOR position{};
    VECTOR scale{ONE, ONE, ONE};
};

struct TexRegion {
    int u0, v0; // top left
    int u3, v3; // bottom right
};

struct Quad {
    SVECTOR vs[4];
};

struct Work {
    SVECTOR ov[4];
    CVECTOR ouv[4];
    CVECTOR ocol[4];

    SVECTOR v[4];
    CVECTOR uv[4];
    CVECTOR col[4];

    CVECTOR intCol;
};

struct Work2 {
    SVECTOR ov[4];
    CVECTOR ouv[4];
    CVECTOR ocol[4];

    SVECTOR v[4];
    CVECTOR uv[4];
    CVECTOR col[4];

    CVECTOR intCol;

    SVECTOR oov[4];
    CVECTOR oouv[4];
    CVECTOR oocol[4];
};

class Game {
public:
    void init();
    void run();
    void handleInput();
    void update();
    void draw();

    void display();

private:
    void drawModel(
        Object& object,
        const Model& model,
        std::uint16_t textureIdx,
        bool subdivide = false);
    void drawModelFast(Object& object, const FastModelInstance& mesh);
    void drawMesh(Object& object, const Mesh& mesh, std::uint16_t textureIdx, bool subdivide);
    void drawQuads(const Mesh& mesh, u_long tpage, int clut);
    std::uint16_t addTexture(TIM_IMAGE texture);

    DISPENV dispEnv[2];
    DRAWENV drawEnv[2];

    static constexpr int OTLEN = 1 << 12;
    static constexpr int PRIMBUFFLEN = 32768 * 4;
    u_long ot[2][OTLEN];
    char primbuff[2][PRIMBUFFLEN];

    char* nextpri{nullptr};
    short currBuffer{0};

    RECT screenClip;

    std::uint16_t bricksTextureIdx;
    std::uint16_t rollTextureIdx;

    Object roll;
    static constexpr int numRolls{5};
    FastModel rollModel;
    FastModelInstance rollModelInstances[numRolls];

    Object level;
    Model levelModel;

    eastl::vector<TIM_IMAGE> textures;

    MATRIX worldmat{};
    MATRIX viewmat{};

    VECTOR tpos{}; // Translation value for matrix calculations
    SVECTOR trot{}; // Rotation value for matrix calculations

    Camera camera;
};
