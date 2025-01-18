#include <math.hexpat>

struct Vec3WithPadding {
    FixedPoint<s16> x, y, z;
    u16 pad [[hidden]];
} [[sealed, format("format_vec3")]];

struct GouraudTriangle {
    u8 r0, g0, b0, code;
    s16 x0, y0;

    u8 r1, g1, b1, p1;
    s16 x1, y1;

    u8 r2, g2, b2, p2;
    s16 x2, y2;
};

struct GouraudQuad {
    u8 r0, g0, b0, code;
    s16 x0, y0;

    u8 r1, g1, b1, p1;
    s16 x1, y1;

    u8 r2, g2, b2, p2;
    s16 x2, y2;

    u8 r3, g3, b3, p3;
    s16 x3, y3;
};

struct GouraudTexturedTriangle {
    u8 r0, g0, b0, code;
    s16 x0, y0;

    u8 u0, v0;
    u16 clut;

    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;

    u8 r2, g2, b2, p2;
    s16 x2, y2;
    u8 u2, v2;
    u16 bias; // actually padding
};

struct GouraudTexturedQuad {
    u8 r0, g0, b0, code;
    s16 x0, y0;

    u8 u0, v0;
    u16 clut;

    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;

    u8 r2, g2, b2, p2;
    s16 x2, y2;
    u8 u2, v2;
    s16 bias; // actually u16 pad2

    u8 r3, g3, b3, p3;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad3;
};

struct G3Data {
    GouraudTriangle tri;
};

struct G4Data {
    GouraudQuad quad;
};

struct GT3Data {
    GouraudTexturedTriangle tri;
};

struct GT4Data {
    GouraudTexturedQuad quad;
};

struct Submesh {
    u16 joint_id;
    u16 untextured_tri_num;
    u16 untextured_quad_num;
    u16 tri_num;
    u16 quad_num;
    
    Vec3WithPadding vs[3 * untextured_tri_num + 4 * untextured_quad_num + 3 * tri_num + 4 * quad_num];
    
    G3Data untextured_tris[untextured_tri_num];
    G4Data untextured_quads[untextured_quad_num];
    GT3Data tris[tri_num];
    GT4Data quads[quad_num];
};

struct Joint {
    Vec3 translation;
    s16 pad;
    Quaternion rotation;
    u8 first_child;
    u8 next_sibling;
};

struct Armature {
    u16 numJoints;
    Joint joints[numJoints];
};

bitfield ModelFlags {
    bool hasArmature: 1;
    unsigned unused: 15;
};

struct Model {
    ModelFlags modelFlags;
    u16 submeshes_size;
    Submesh submeshes[submeshes_size];
    if (modelFlags.hasArmature) {
        Armature armature;
    }
};

Model model @ 0x0;
