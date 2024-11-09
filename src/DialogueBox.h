#pragma once

#include <psyqo/primitives/common.hh>

#include <EASTL/fixed_string.h>

#include "Bouncer.h"
#include "Renderer.h"
#include "Timer.h"

namespace psyqo
{
struct SimplePad;
};

struct TextureInfo;

struct GlyphInfo {
    int u;
    int v;
};

class DialogueBox {
public:
    DialogueBox();
    void handleInput(const psyqo::SimplePad& pad);
    void update();
    void draw(Renderer& renderer, const TextureInfo& fontAtlasTexture);

private:
    void drawBG(Renderer& renderer, const TextureInfo& fontAtlasTexture);
    void drawText(Renderer& renderer, const TextureInfo& fontAtlasTexture);
    void drawMoreTextIndicator(Renderer& renderer, const TextureInfo& fontAtlasTexture);

    void drawLine(
        Renderer::PrimBufferAllocatorType& primBuffer,
        psyqo::GPU& gpu,
        int16_t aX,
        int16_t aY,
        int16_t bX,
        int16_t bY,
        const psyqo::Color& color);

    psyqo::Vertex position{{.x = 60, .y = 140}};
    psyqo::Vertex size{{.x = 200, .y = 64}};

    eastl::fixed_string<char, 256> textString{"AAAAAAAAAAAA\nBB \2AA \1BB"};
    int numOfCharsToShow{0};
    psyqo::Vertex textOffset{{.x = 0, .y = 0}};
    psyqo::Vertex glyphSize{{.x = 16, .y = 16}};

    GlyphInfo glyphInfos[255];
    bool allTextShown{false};

    Timer letterIncrementTimer{2};
    Bouncer moreTextBouncer{4, 2};
    // offset from the bottom-right corner of dialogue box
    psyqo::Vertex moreTextOffset{{.x = 14, .y = 10}};
};
