/*  primdrawG.c, by Schnappy, 12-2020
    - Draw a gouraud shaded mesh exported as a TMESH by the blender <= 2.79b plugin
   io_export_psx_tmesh.py based on primdraw.c by Lameguy64
   (http://www.psxdev.net/forum/viewtopic.php?f=64&t=537) 2014 Meido-Tek Productions. Controls:
        Start                           - Toggle interactive/non-interactive mode.
        Select                          - Reset object's position and angles.
        L1/L2                           - Move object closer/farther.
        L2/R2                           - Rotate object (XY).
        Up/Down/Left/Right              - Rotate object (XZ/YZ).
        Triangle/Cross/Square/Circle    - Move object up/down/left/right.
*/

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>

#include <stdio.h>

// Sample vector model
#include "cube.h"

/* C++ header tests...

#include <cstdint>

typedef void* __gnuc_va_list;
#define __time_t_defined
#include <memory>

*/

namespace
{
constexpr bool VMODE = false;
constexpr int SCREENXRES = 320;
constexpr int SCREENYRES = 240;
constexpr int CENTERX = SCREENXRES / 2;
constexpr int CENTERY = SCREENYRES / 2;

constexpr int OTLEN = 2048;
constexpr int PRIMBUFFLEN = 32768;

DISPENV disp[2];
DRAWENV draw[2];

u_long ot[2][OTLEN];
char primbuff[2][PRIMBUFFLEN];
char* nextpri = primbuff[0];
short currBuffer = 0;

} // end of anonymous namespace

void init()
{
    // Reset the GPU before doing anything and the controller
    PadInit(0);
    ResetGraph(0);

    // Initialize and setup the GTE
    InitGeom();
    SetGeomOffset(CENTERX, CENTERY); // x, y offset
    SetGeomScreen(CENTERX); // Distance between eye and screen

    // Set the display and draw environments
    SetDefDispEnv(&disp[0], 0, 0, SCREENXRES, SCREENYRES);
    SetDefDispEnv(&disp[1], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&draw[0], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&draw[1], 0, 0, SCREENXRES, SCREENYRES);

    printf("Hello from printf!\n");

    if (VMODE) {
        SetVideoMode(MODE_PAL);
        disp[0].screen.y += 8;
        disp[1].screen.y += 8;
    }

    SetDispMask(1); // Display on screen

    setRGB0(&draw[0], 0, 128, 255);
    setRGB0(&draw[1], 0, 128, 255);
    draw[0].isbg = 1;
    draw[1].isbg = 1;

    PutDispEnv(&disp[currBuffer]);
    PutDrawEnv(&draw[currBuffer]);

    // Init font system
    FntLoad(960, 0);
    FntOpen(16, 16, 196, 64, 0, 256);
}

void display(void)
{
    DrawSync(0);
    VSync(0);
    PutDispEnv(&disp[currBuffer]);
    PutDrawEnv(&draw[currBuffer]);
    DrawOTag(&ot[currBuffer][OTLEN - 1]);
    currBuffer = !currBuffer;
    nextpri = primbuff[currBuffer];
}

int main()
{
    bool startPressed = false;
    bool autoRotate = true;

    auto rotation = SVECTOR{232, 232, 0, 0};
    auto translation = VECTOR{0, 0, CENTERX * 3, 0};
    auto scale = VECTOR{ONE / 2, ONE / 2, ONE / 2, 0};
    auto matrix = MATRIX{};

    init();

    // Main loop
    while (true) {
        // Read pad status
        const auto PadStatus = PadRead(0);
        if (!autoRotate) {
            if (PadStatus & PADL1) translation.vz -= 4;
            if (PadStatus & PADR1) translation.vz += 4;
            if (PadStatus & PADL2) rotation.vz -= 8;
            if (PadStatus & PADR2) rotation.vz += 8;
            if (PadStatus & PADLup) rotation.vx -= 8;
            if (PadStatus & PADLdown) rotation.vx += 8;
            if (PadStatus & PADLleft) rotation.vy -= 8;
            if (PadStatus & PADLright) rotation.vy += 8;
            if (PadStatus & PADRup) translation.vy -= 2;
            if (PadStatus & PADRdown) translation.vy += 2;
            if (PadStatus & PADRleft) translation.vx -= 2;
            if (PadStatus & PADRright) translation.vx += 2;
            if (PadStatus & PADselect) {
                rotation.vx = rotation.vy = rotation.vz = 0;
                scale.vx = scale.vy = scale.vz = ONE / 2;
                translation.vx = translation.vy = 0;
                translation.vz = CENTERX * 3;
            }
        }

        if (PadStatus & PADstart) {
            if (!startPressed) {
                autoRotate = (autoRotate + 1) & 1;
                rotation.vx = rotation.vy = rotation.vz = 0;
                scale.vx = scale.vy = scale.vz = ONE / 2;
                translation.vx = translation.vy = 0;
                translation.vz = CENTERX * 3;
            }
            startPressed = true;
        } else {
            startPressed = false;
        }

        if (autoRotate) {
            rotation.vy += 28; // Pan
            rotation.vx += 28; // Tilt
        }

        ClearOTagR(ot[currBuffer], OTLEN);

        RotMatrix(&rotation, &matrix);
        TransMatrix(&matrix, &translation);
        ScaleMatrix(&matrix, &scale);
        SetRotMatrix(&matrix);
        SetTransMatrix(&matrix);

        for (int i = 0; i < modelCube.len * 3; i += 3) {
            long p, otz, flags;
            const auto poly = (POLY_G3*)nextpri;
            SetPolyG3(poly);

            setRGB0(poly, modelCube.c[i].r, modelCube.c[i].g, modelCube.c[i].b);
            setRGB1(poly, modelCube.c[i + 2].r, modelCube.c[i + 2].g, modelCube.c[i + 2].b);
            setRGB2(poly, modelCube.c[i + 1].r, modelCube.c[i + 1].g, modelCube.c[i + 1].b);

            const auto nclip = RotAverageNclip3(
                &modelCube_mesh[modelCube_index[i]],
                &modelCube_mesh[modelCube_index[i + 2]],
                &modelCube_mesh[modelCube_index[i + 1]],
                (long*)&poly->x0,
                (long*)&poly->x1,
                (long*)&poly->x2,
                &p,
                &otz,
                &flags);
            if (nclip <= 0) {
                continue;
            }
            if ((otz > 0) && (otz < OTLEN)) {
                addPrim(&ot[currBuffer][otz - 2], poly);
            }
            nextpri += sizeof(POLY_G3);
        }

        FntPrint("Hello gouraud shaded cube!\n");
        FntFlush(-1);

        display();
    }

    return 0;
}
