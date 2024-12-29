import std.string;
import std.math;
import std.core;
import std.io;
import std.sys;

fn fixed16_to_float(auto fp16) {
    return fp16.value / 4096.f;
};

fn format_fixed16(auto fp16) {
    return fixed16_to_float(fp16);
};

fn format_quat(auto quat) {
    return std::format("({:.3f}, {:.3f}, {:.3f}, {:.3f})", 
        fixed16_to_float(quat.w),
        fixed16_to_float(quat.x),
        fixed16_to_float(quat.y),
        fixed16_to_float(quat.z));
};

fn format_vec3(auto v) {
    return std::format("({:.3f}, {:.3f}, {:.3f})", 
        fixed16_to_float(v.x),
        fixed16_to_float(v.y),
        fixed16_to_float(v.z));
};

fn format_color(auto c) {
    return std::format("({}, {}, {})", 
        c.r,
        c.g,
        c.b);
};

// 4.12
struct FixedPoint16 {
    s16 value;
} [[sealed, format("format_fixed16")]];

struct Vec3 {
    FixedPoint16 x, y, z;
} [[sealed, format("format_vec3")]];

struct Vec3WithPadding {
    FixedPoint16 x, y, z;
    u16 pad [[hidden]];
} [[sealed, format("format_vec3")]];

struct Quaternion {
    FixedPoint16 w, x, y, z;
} [[sealed, format("format_quat")]];

struct RGB8 {
  u8 r, g, b;
} [[sealed, static, format("format_color"), color(std::format("{:02X}{:02X}{:02X}", r*2, g*2, b*2))]];


struct GouraudTexturedQuad {
    u32 tag;

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
    u16 pad2;

    u8 r3, g3, b3, p3;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad3;
};

struct GT4Data {
    GouraudTexturedQuad quad;
    Vec3WithPadding vs[4];
};

struct Submesh {
    u16 joint_id;
    u16 untextured_tri_num;
    u16 untextured_quad_num;
    u16 tri_num;
    u16 quad_num;
    
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

struct Model {
    u16 submeshes_size;
    Submesh submeshes[submeshes_size];
    if (!std::mem::eof()) {
        Armature armature;
    }
};

Model model @ 0x0;