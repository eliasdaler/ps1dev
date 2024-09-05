#pragma once

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include <cstdint>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include "Camera.h"

inline constexpr int SCREENXRES = 320;
inline constexpr int SCREENYRES = 240;

struct Object {
    SVECTOR rotation;
    VECTOR position;
    VECTOR scale;
};

struct SVEC2 {
    std::uint8_t x, y;
};

struct Mesh {
    eastl::vector<SVECTOR> vertices;
    eastl::vector<SVEC2> uvs;
    int numTris;
    int numQuads;
};

struct Model {
    eastl::vector<std::uint16_t> meshesIndices;
};

struct TexRegion {
    int u0, v0; // top left
    int u3, v3; // bottom right
};

struct Quad {
    SVECTOR v0;
    SVECTOR v1;
    SVECTOR v2;
    SVECTOR v3;
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

    void drawModel(Object& object, const Model& model, std::uint16_t textureIdx);
    void drawMesh(Object& object, const Mesh& mesh, std::uint16_t textureIdx);

    void drawLevel();

    void drawTile(
        Object& object,
        std::uint16_t meshIdx,
        std::uint16_t textureIdx,
        const TexRegion& uvs);

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
    static constexpr int PRIMBUFFLEN = 32768 * 2;
    u_long ot[2][OTLEN];
    char primbuff[2][PRIMBUFFLEN];

    char* nextpri{nullptr};
    short currBuffer{0};

    int CENTERX{SCREENXRES / 2};
    int CENTERY{SCREENYRES / 2};

    RECT screenClip;

    Object roll;
    Object floorTileObj;
    Object wallTileObj;

    Mesh tileMesh; // we use it for transforming tiles on CPU

    std::uint16_t bricksTextureIdx;
    std::uint16_t floorTextureIdx;
    std::uint16_t rollTextureIdx;

    Model rollModel;

    std::uint16_t floorMeshIdx;
    std::uint16_t wallMeshIdx;
    std::uint16_t wallMeshLIdx;
    std::uint16_t wallMeshRIdx;

    eastl::vector<Mesh> meshes;
    eastl::vector<TIM_IMAGE> textures;

    MATRIX worldmat = {0};
    MATRIX viewmat = {0};

    VECTOR tpos; // Translation value for matrix calculations
    SVECTOR trot; // Rotation value for matrix calculations

    Camera camera;
};
