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

struct Quaternion {
    FixedPoint16 w, x, y, z;
} [[sealed, format("format_quat")]];

struct RGB8 {
  u8 r, g, b;
} [[sealed, static, format("format_color"), color(std::format("{:02X}{:02X}{:02X}", r*2, g*2, b*2))]];

struct Vertex {
    Vec3 pos;
    u16 pad1;
    
    u8 uv[2];
    u16 pad2;
    
    RGB8 color;
    u8 pad3;
};

struct Tri {
    Vertex vertices[3];
};

struct Quad {
    Vertex vertices[4];
};

struct Submesh {
    u16 joint_id;
    u16 untextured_tri_num;
    u16 untextured_quad_num;
    u16 tri_num;
    u16 quad_num;
    
    Tri untextured_tris[untextured_tri_num];
    Quad untextured_quads[untextured_quad_num];
    Tri tris[tri_num];
    Quad quads[quad_num];
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
