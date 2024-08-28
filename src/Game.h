#pragma once

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include <EASTL/vector.h>

#include "Camera.h"

inline constexpr int SCREENXRES = 320;
inline constexpr int SCREENYRES = 240;

struct Object {
    SVECTOR rotation;
    VECTOR position;
    VECTOR scale;

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
    void drawObject(Object& object, TIM_IMAGE& texture);
    void display();

private:
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
    TIM_IMAGE bricksTexture;

    Object floorTile;
    TIM_IMAGE floorTexture;

    Object wallTile;
    TIM_IMAGE fTexture;

    MATRIX worldmat = {0};
    MATRIX viewmat = {0};

    Camera camera;
};
