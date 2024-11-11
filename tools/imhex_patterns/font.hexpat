#pragma once

import std.io;
import std.core;

struct Glyph {
    u16 codepoint;
    u8 uv0[2];
    u8 size[2];
    u8 bearing[2];
    u8 advance;
    u8 _pad1;
};

struct Font {
    u16 magic;
    u8 lineSpacing;
    u8 ascenderPx;
    u8 descenderPx;
    u8 _pad0;
    u16 glyphsNum;
    Glyph glyphs[glyphsNum];
};

Font font @ 0x0;