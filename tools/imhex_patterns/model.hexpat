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

// 4.12
struct FixedPoint16 {
    s16 value;
} [[format("format_fixed16")]];

struct Quaternion {
    FixedPoint16 w, x, y, z;
} [[format("format_quat")]];

struct Vertex {
    FixedPoint16 pos[3];
    s16 pad1;
    
    u8 uv[2];
    u16 pad2;
    
    u8 color[3];
    u8 pad3;
};

struct Tri {
    Vertex vertices[3];
};

struct Quad {
    Vertex vertices[4];
};

struct Submesh {
    u16 subdivide;
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
    FixedPoint16 pos[3];
    s16 pad;
    Quaternion quat;
    u8 first_child;
    u8 next_sibling;
};

struct Armature {
    u16 numJoints;
    Joint joints[numJoints];
    u16 influenceSizes[numJoints];
    u16 influenceIndices[32]; // TODO: calculate how many elements there are
};

struct Model {
    u16 submeshes_size;
    Submesh submeshes[submeshes_size];
    Armature armature;
};

Model model @ 0x0;