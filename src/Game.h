#pragma once

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

inline constexpr int SCREENXRES = 320;
inline constexpr int SCREENYRES = 240;

class Game {
public:
    void init();
    void run();
    void handleInput();
    void update();
    void draw();
    void drawCube(const TMESH& cube);
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

    bool startPressed = false;
    bool autoRotate = true;

    SVECTOR rotation;
    VECTOR translation;
    VECTOR scale;
    MATRIX matrix;

    int CENTERX{SCREENXRES / 2};
    int CENTERY{SCREENYRES / 2};
};
