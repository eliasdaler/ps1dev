#pragma once

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include <cstdint>

#include <EASTL/vector.h>

#include "Camera.h"

inline constexpr int SCREENXRES = 320;
inline constexpr int SCREENYRES = 240;

struct Object {
    SVECTOR rotation;
    VECTOR position;
    VECTOR scale;

    std::uint16_t meshIdx;
    std::uint16_t textureIdx;
};

struct Mesh {
    eastl::vector<SVECTOR> vertices;
    eastl::vector<short> faces;
    eastl::vector<CVECTOR> colors;
};

class Game {
public:
    void init();
    void run();
    void handleInput();
    void update();
    void draw();
    void drawObject(Object& object, bool cpuTrans = false, Mesh* cpuMesh = nullptr);
    void display();

private:
    std::uint16_t addMesh(Mesh mesh);
    std::uint16_t addTexture(TIM_IMAGE texture);

    DISPENV dispEnv[2];
    DRAWENV drawEnv[2];

    static constexpr int OTLEN = 2048;
    static constexpr int PRIMBUFFLEN = 32768;
    u_long ot[2][OTLEN];
    char primbuff[2][PRIMBUFFLEN];

    char* nextpri{nullptr};
    short currBuffer{0};

    int CENTERX{SCREENXRES / 2};
    int CENTERY{SCREENYRES / 2};

    Object cube;
    Object floorTileObj;
    Object wallTileObj;

    Mesh tileMesh; // we use it for transforming tiles on CPU

    std::uint16_t bricksTextureIdx;
    std::uint16_t floorTextureIdx;
    std::uint16_t fTextureIdx;

    std::uint16_t cubeMeshIdx;
    std::uint16_t floorMeshIdx;
    std::uint16_t wallMeshIdx;

    eastl::vector<Mesh> meshes;
    eastl::vector<TIM_IMAGE> textures;

    MATRIX worldmat = {0};
    MATRIX viewmat = {0};

    Camera camera;
};
