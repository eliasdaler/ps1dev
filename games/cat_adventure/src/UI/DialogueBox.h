#pragma once

#include <psyqo/primitives/common.hh>

#include <EASTL/fixed_string.h>

#include <Core/Timer.h>
#include <Graphics/Renderer.h>
#include <Util/Bouncer.h>

struct TextureInfo;
struct Font;
class PadManager;

class DialogueBox {
public:
    DialogueBox();
    void handleInput(const PadManager& pad);
    void update();
    void draw(Renderer& renderer,
        const Font& font,
        const TextureInfo& fontAtlasTexture,
        const TextureInfo& borderTexture);

    void setText(const char* text, bool displayImmediately = false);

    bool displayBorders{true};
    bool displayMoreTextArrow{true};

    psyqo::Vertex position{{.x = 60, .y = 140}};
    psyqo::Vertex size{{.x = 200, .y = 64}};
    psyqo::Vertex textOffset{{.x = 2, .y = 0}};

    bool isOpen{false};
    bool wantClose{false};

private:
    void drawBG(Renderer& renderer, const TextureInfo& borderTexture);
    void drawBorders(Renderer& renderer, const TextureInfo& borderTexture);
    void drawText(Renderer& renderer, const Font& font, const TextureInfo& fontAtlasTexture);
    void drawMoreTextIndicator(Renderer& renderer, const TextureInfo& borderTexture);

    void drawLine(Renderer::PrimBufferAllocatorType& primBuffer,
        psyqo::GPU& gpu,
        int16_t aX,
        int16_t aY,
        int16_t bX,
        int16_t bY,
        const psyqo::Color& color);

    eastl::fixed_string<char, 256> textString;
    int numOfCharsToShow{0};

    bool allTextShown{false};

    Timer letterIncrementTimer{2};
    Bouncer moreTextBouncer{4, 2};
    // offset from the bottom-right corner of dialogue box
    psyqo::Vertex moreTextOffset{{.x = 14, .y = 10}};

    // wavy text
    Timer waveTimer{6};
    int wavePhase{0};
    int waveMax{5};
    int waveOffsets[6] = {-2, -1, 0, 1, 0, -1};
};
