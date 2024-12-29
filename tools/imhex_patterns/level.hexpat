import std.string;

fn fixed16_to_float(auto fp16) {
    return fp16.value / 4096.f;
};

fn format_fixed16(auto fp16) {
    return fixed16_to_float(fp16);
};

fn format_vec2(auto v) {
    return std::format("({:.3f}, {:.3f})", 
        fixed16_to_float(v.x),
        fixed16_to_float(v.y));
};

fn format_vec3(auto v) {
    return std::format("({:.3f}, {:.3f}, {:.3f})", 
        fixed16_to_float(v.x),
        fixed16_to_float(v.y),
        fixed16_to_float(v.z));
};

struct FixedPoint<T> {
    T value;
} [[sealed, format("format_fixed16")]];

struct Vec2 {
    FixedPoint<s16> x, y;
} [[sealed, format("format_vec2")]];

struct Vec3 {
    FixedPoint<s16> x, y, z;
} [[sealed, format("format_vec3")]];

struct AABB {
    Vec2 min;
    Vec2 max;
};

using StringHash = u32;

struct AnimationsFile {
    u16 numUsedTextures;
    StringHash usedTextures[numUsedTextures];
    u16 numUsedModels;
    StringHash usedModels[numUsedModels];
    u16 numColliders;
    AABB colliders[numColliders];
};

AnimationsFile file @ 0x0;