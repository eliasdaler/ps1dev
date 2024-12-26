#pragma once

#include <EASTL/string_view.h>

#include <psyqo/primitives/common.hh>

struct Renderer;
struct Font;
struct TextureInfo;

namespace ui
{
psyqo::Vertex drawTextLabel(
    const psyqo::Vertex& position,
    eastl::string_view str,
    const psyqo::Color color,
    Renderer& renderer,
    const Font& font,
    const TextureInfo& fontAtlasTexture,
    bool setTPage = true);
}
