#include "TextLabel.h"

#include "Font.h"
#include "Renderer.h"

#include <psyqo/primitives/sprites.hh>

namespace ui
{
void drawTextLabel(
    const psyqo::Vertex& position,
    eastl::string_view str,
    const psyqo::Color color,
    Renderer& renderer,
    const Font& font,
    const TextureInfo& fontAtlasTexture,
    bool setTPage)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    if (setTPage) {
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr = fontAtlasTexture.tpage;
        gpu.chain(tpage);
    }

    const psyqo::Vertex glyphPos{{
        .x = static_cast<int16_t>(position.x),
        .y = static_cast<int16_t>(position.y),
    }};

    int lineNum = 0;
    int lineX = 0; // current line X offset

    const auto lineSpacing = font.lineSpacing;
    const auto ascenderPx = font.lineSpacing;
    const auto& glyphs = font.glyphs;
    const auto fontClut = fontAtlasTexture.clut;

    for (const auto c : str) {
        if (c == '\n') {
            ++lineNum;
            lineX = 0;
            continue;
        }

        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = fontClut;

        const auto& glyphInfo = glyphs[(int)c];
        sprite.position.x = glyphPos.x + lineX + glyphInfo.bearing.x;
        sprite.position.y = glyphPos.y + (ascenderPx - glyphInfo.bearing.y) + lineNum * lineSpacing;
        sprite.texInfo.u = glyphInfo.uv0.x;
        sprite.texInfo.v = glyphInfo.uv0.y;
        sprite.size.x = glyphInfo.size.x;
        sprite.size.y = glyphInfo.size.y;

        sprite.setColor(color);

        gpu.chain(spriteFrag);

        lineX += glyphInfo.advance;
    }
}
}
