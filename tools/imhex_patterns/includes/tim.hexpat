#pragma once

#pragma loop_limit  20000000
#pragma array_limit 20000000
#pragma pattern_limit 2000000000

import std.io;
import std.core;



bitfield RGBA5551 {
    r : 5;
    g : 5;
    b : 5;
    a : 1;
} [[sealed,
    format("impl::format_color"),
    color(std::format("{0:02X}{1:02X}{2:02X}", color_to_8bit(r), color_to_8bit(g), color_to_8bit(b)))
  ]];

namespace impl {
    fn format_color(ref RGBA5551 color) {
            return std::format("#{0:02X}{1:02X}{2:02X} | rgba({0}, {1}, {2}, {3})",
                        color_to_8bit(color.r),
                        color_to_8bit(color.g),
                        color_to_8bit(color.b),
                        255);
    };
}

fn color_to_8bit(u8 v) {
    return u8((float(v) / 31.f) * 255.f);
};

fn rgba5ToU32(auto c) {
    return u32(color_to_8bit(c.r) | color_to_8bit(c.g) << 8 | color_to_8bit(c.b) << 16 | 0xFF000000);
};

enum PMode : u8 {
    Clut4Bit,
    Clut8Bit,
    Direct15Bit,
    Direct24Bit,
    Mixed,
};

fn get_num_clut_colors(PMode pmode) {
    if (pmode == PMode::Clut8Bit) {
        return 256;
    }
    return 16;
};

// how many indices stored in one pixel
fn get_pixel_index_count(PMode pmode) {
    if (tim.flag.pmode == PMode::Clut8Bit) {
        return 2;
    } else if (tim.flag.pmode == PMode::Clut4Bit) {
        return 4;
    }
    return 1;
};

bitfield Flag {
  PMode pmode : 3;
  bool hasClut : 1;
  reserved : 32-4;
};


bitfield Pixel4Bit {
   p0: 4;
   p1: 4;
   p2: 4;
   p3: 4;
};

struct Pixel8Bit {
   u8 p0;
   u8 p1;
};

struct Pixel16Bit {
   RGBA5551 col;
};

struct ClutColors<auto N> {
    RGBA5551 colors[N];
};

struct Clut<auto N> {
    u32 bnum;
    u16 dx; u16 dy;
    u16 w; u16 h;
    ClutColors<N> colors[h];
};

struct PixelData<PixelType> {
    u32 bnum;
    u16 dx; u16 dy;
    u16 w; u16 h;
    PixelType indices[w*h];
};

struct TimFile {
    u32 magic;
    Flag flag;

    if (flag.pmode == PMode::Clut4Bit) {
        Clut<16> clut;
    } else if(flag.pmode == PMode::Clut8Bit) {
        Clut<256> clut;
    }

    if (flag.pmode == PMode::Clut4Bit) {
        PixelData<Pixel4Bit> pixels;
    } else if (flag.pmode == PMode::Clut8Bit) {
        PixelData<Pixel8Bit> pixels;
    } else {
        PixelData<Pixel16Bit> pixels;
    }
};

TimFile tim @ 0x0;

// now, let's visualize it
const u8 pixelSize = get_pixel_index_count(tim.flag.pmode);
const u16 imageWidth = tim.pixels.w * pixelSize;
const u16 imageHeight = tim.pixels.h;

struct Image {
	u32 rgba[imageWidth * imageHeight];
} [[hex::visualize("bitmap", rgba, imageWidth, imageHeight)]];

std::mem::Section section = std::mem::create_section("Section");
std::mem::set_section_size(section, imageWidth * imageHeight * sizeof(u32));
Image image @ 0x00 in section;

const u16 num_clut_colors =  get_num_clut_colors(tim.flag.pmode);
u32 clut[num_clut_colors];

fn set_pixels_from_clut(PMode pmode, ref auto indices, u16 i) {
    image.rgba[i + 0] = clut[indices.p0];
    image.rgba[i + 1] = clut[indices.p1];
    if (pmode == PMode::Clut4Bit) {
        image.rgba[i + 2] = clut[indices.p2];
        image.rgba[i + 3] = clut[indices.p3];
    }
};

fn copy_clut(ref auto clutRef) {
    for (u16 i = 0, i < num_clut_colors, i = i + 1) {
        clut[i] = rgba5ToU32(clutRef.colors[i]);
    }
};

u32 clut_to_inspect in; // "Section".image will be displayed with this CLUT
u32 num_cluts out; // num_cluts for ease of debug
bool visualize in; // if true - produce "Section".image (can be very slow!)

fn main() {
    if (tim.flag.pmode == PMode::Direct24Bit || tim.flag.pmode == PMode::Mixed) {
        // TODO?
        return;
    }

    if (!visualize) {
        return;
    }

    if (tim.flag.hasClut) {
        num_cluts = tim.clut.h;
        copy_clut(tim.clut.colors[clut_to_inspect]);
    }

    const u64 numPixels = sizeof(tim.pixels.indices) / 2;
    for (u64 x = 0, x < numPixels, x = x + 1) {
        u32 i =  pixelSize * x;
        if (tim.flag.pmode == PMode::Direct15Bit) {
            image.rgba[i] = rgba5ToU32(tim.pixels.indices[x].col);
        } else {
            set_pixels_from_clut(tim.flag.pmode, tim.pixels.indices[x], i);
        }
    }
};
