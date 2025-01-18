#pragma once

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

fn format_color(auto c) {
    return std::format("({}, {}, {})", 
        c.r,
        c.g,
        c.b);
};

// 4.12
struct FixedPoint<T> {
    T value;
} [[sealed, format("format_fixed16")]];

struct Vec2 {
    FixedPoint<s16> x, y;
} [[sealed, format("format_vec2")]];

struct Vec3 {
    FixedPoint<s16> x, y, z;
} [[sealed, format("format_vec3")]];

struct Quaternion {
    FixedPoint<s16> w, x, y, z;
} [[sealed, format("format_quat")]];

struct RGB8 {
  u8 r, g, b;
} [[sealed, static, format("format_color"), color(std::format("{:02X}{:02X}{:02X}", r*2, g*2, b*2))]];

