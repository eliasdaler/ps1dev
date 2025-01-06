#include <math.hexpat>

struct Key<auto keyType> {
    FixedPoint<s32> frame;
    if (keyType == 0) {
        Quaternion rotation;
    } else if (keyType == 1) {
        Vec3 translation;
        s16 pad [[hidden]];
    }
};

struct Track {
    u8 type;
    u8 jointId;
    u16 numKeys;
    Key<type> keys[numKeys];
};

bitfield AnimationFlags {
    bool looped: 1;
    unsigned unused: 31;
};
    
struct Animation {
    u32 name;
    AnimationFlags flags;
    u16 length;
    u16 numTracks;
    Track tracks[numTracks];
};

struct AnimationsFile {
    u32 numAnimations;
    Animation animations[numAnimations];
};

AnimationsFile file @ 0x0;
