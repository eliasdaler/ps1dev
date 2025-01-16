How to draw subdiv meshes (only quads for now)

```cpp
    auto& wrk1 = *(SubdivData1*)(SCRATCH_PAD);
    wrk1.ov[0] = v0;
    wrk1.ov[1] = v1;
    wrk1.ov[2] = v2;
    wrk1.ov[3] = v3;

#ifdef FULL_INLINE
    wrk1.ouv[0].u = prim.uvA.u;
    wrk1.ouv[0].v = prim.uvA.v;

    wrk1.ouv[1].u = prim.uvB.u;
    wrk1.ouv[1].v = prim.uvB.v;

    wrk1.ouv[2].u = prim.uvC.u;
    wrk1.ouv[2].v = prim.uvC.v;

    wrk1.ouv[3].u = prim.uvD.u;
    wrk1.ouv[3].v = prim.uvD.v;

    wrk1.ocol[0] = quad2d.getColorA();
    wrk1.ocol[1] = quad2d.colorB;
    wrk1.ocol[2] = quad2d.colorC;
    wrk1.ocol[3] = quad2d.colorD;

    const auto tpage = quad2d.tpage;
    const auto clut = quad2d.clutIndex;
    const auto command = quad2d.command;

    if (avgZ < LEVEL_2_SUBDIV_DIST) {
        auto& wrk2 = *(SubdivData2*)(SCRATCH_PAD);
        for (int i = 0; i < 4; ++i) {
            wrk2.oov[i] = wrk2.ov[i];
            wrk2.oouv[i] = wrk2.ouv[i];
            wrk2.oocol[i] = wrk2.ocol[i];
        }

        DRAW_QUADS_44(wrk2);
        continue;
    }

    DRAW_QUADS_22(wrk1);
#else
    drawQuadSubdiv(quad2d, avgZ, addBias);
#endif
    continue;
}
```
