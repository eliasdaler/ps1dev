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

inline constexpr int SCREENXRES = 320;
inline constexpr int SCREENYRES = 240;

struct Object {
    SVECTOR rotation{};
    VECTOR position{};
    VECTOR scale{ONE, ONE, ONE};
};

struct Vertex {
    std::int16_t x, y, z;
    std::uint8_t u, v;
    std::uint8_t r, g, b;
};

struct Mesh {
    eastl::vector<Vertex> vertices;
    int numTris{0};
    int numQuads{0};
};

struct Model {
    eastl::vector<std::uint16_t> meshesIndices;
};

struct FastVertex {
    std::int16_t x, y, z;
};

struct FastModel {
    eastl::unique_ptr<FastVertex> vertexData{nullptr};
    std::uint16_t numVertices{0};
    eastl::unique_ptr<std::uint8_t> primData{nullptr};
    std::uint16_t numTris{0};
    std::uint16_t numQuads{0};
};

struct FastModelInstance {
    eastl::span<FastVertex> vertices;
    eastl::unique_ptr<std::uint8_t> primData{nullptr};
    eastl::span<POLY_GT3> trianglePrims[2];
    eastl::span<POLY_GT4> quadPrims[2];
};

struct TexRegion {
    int u0, v0; // top left
    int u3, v3; // bottom right
};

struct Quad {
    SVECTOR vs[4];
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
    void loadModel(Model& model, eastl::string_view filename);
    void loadFastModel(FastModel& model, eastl::string_view filename);

    FastModelInstance makeFastModelInstance(FastModel& model, const TIM_IMAGE& texture);

    void drawModel(
        Object& object,
        const Model& model,
        std::uint16_t textureIdx,
        bool subdivide = false);
    void drawModelFast(Object& object, const FastModelInstance& mesh);
    void drawMesh(Object& object, const Mesh& mesh, std::uint16_t textureIdx, bool subdivide);
    void drawQuadRecursive(
        Object& object,
        const Quad& quad,
        const TexRegion& uvs,
        const TIM_IMAGE& texture,
        int depth);

    std::uint16_t addMesh(Mesh mesh);
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

    eastl::vector<Mesh> meshes;
    eastl::vector<TIM_IMAGE> textures;

    MATRIX worldmat{};
    MATRIX viewmat{};

    VECTOR tpos{}; // Translation value for matrix calculations
    SVECTOR trot{}; // Rotation value for matrix calculations

    Camera camera;
};
