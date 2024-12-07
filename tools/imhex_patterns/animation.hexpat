import std.string;

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

struct FixedPoint<T> {
    T value;
} [[format("format_fixed16")]];

struct Vec3 {
    FixedPoint<s16> x, y, z;
} [[format("format_vec3")]];

struct Quaternion {
    FixedPoint<s16> w, x, y, z;
} [[format("format_quat")]];

struct Key<auto keyType> {
    FixedPoint<s32> frame;
    if (keyType == 0) {
        Quaternion rotation;
    } else if (keyType == 1) {
        Vec3 translation;
        s16 pad;
    }
};

struct Track {
    u8 type;
    u8 jointId;
    u16 numKeys;
    Key<type> keys[numKeys];
};

struct Animation {
    u32 numTracks;
    Track tracks[numTracks];
};

struct AnimationsFile {
    u32 numAnimations;
    Animation animations[numAnimations];
};

AnimationsFile file @ 0x0;