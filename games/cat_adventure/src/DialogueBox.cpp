#include "DialogueBox.h"

#include "Font.h"
#include "PadManager.h"
#include "RainbowColors.h"
#include "Renderer.h"

#include <psyqo/primitives/lines.hh>
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/primitives/sprites.hh>

DialogueBox::DialogueBox()
{}

void DialogueBox::handleInput(const PadManager& pad)
{
    if (pad.wasButtonJustPressed(psyqo::SimplePad::Cross)) {
        if (allTextShown) {
            wantClose = true;
        } else {
            numOfCharsToShow = textString.size();
        }
    }
}

void DialogueBox::update()
{
    letterIncrementTimer.update();
    if (letterIncrementTimer.tick()) {
        ++numOfCharsToShow;
    }
    if (allTextShown) {
        moreTextBouncer.update();
    }

    // wavy text
    waveTimer.update();
    if (waveTimer.tick()) {
        ++wavePhase;
        if (wavePhase > waveMax) {
            wavePhase = 0;
        }
    }
}

void DialogueBox::draw(
    Renderer& renderer,
    const Font& font,
    const TextureInfo& fontAtlasTexture,
    const TextureInfo& borderTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    // set tpage
    {
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr = borderTexture.tpage;
        gpu.chain(tpage);
    }

    drawBG(renderer, borderTexture);

    if (displayBorders) {
        drawBorders(renderer, borderTexture);
    }

    if (displayMoreTextArrow) {
        drawMoreTextIndicator(renderer, borderTexture);
    }

    // set tpage
    {
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr = fontAtlasTexture.tpage;
        gpu.chain(tpage);
    }

    drawText(renderer, font, fontAtlasTexture);
}

void DialogueBox::drawBG(Renderer& renderer, const TextureInfo& borderTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    { // bg rect
        auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
        auto& rect = rectFrag.primitive;
        rect.position = position;
        rect.size = size;

        static const auto black = psyqo::Color{{0, 0, 0}};
        rect.setColor(black);
        rect.setSemiTrans();
        gpu.chain(rectFrag);
    }
}

void DialogueBox::drawBorders(Renderer& renderer, const TextureInfo& borderTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    static const psyqo::Color dbBorderColors[5] =
        {psyqo::Color{{24, 30, 37}}, // black
         psyqo::Color{{116, 63, 57}}, // brown
         psyqo::Color{{254, 174, 52}}, // orange
         psyqo::Color{{254, 231, 97}}, // yellow
         psyqo::Color{{24, 30, 37}}}; // black

    { // top left
        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = borderTexture.clut;

        sprite.position.x = position.x - 5;
        sprite.position.y = position.y - 5;
        sprite.texInfo.u = 32;
        sprite.texInfo.v = 144;
        sprite.size.x = 5;
        sprite.size.y = 5;

        gpu.chain(spriteFrag);
    }

    { // top
        for (int i = 0; i < 5; ++i) {
            drawLine(
                primBuffer,
                gpu,
                position.x,
                position.y - 5 + i,
                position.x + size.x,
                position.y - 5 + i,
                dbBorderColors[i]);
        }
    }

    { // top right
        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = borderTexture.clut;

        sprite.position.x = position.x + size.x;
        sprite.position.y = position.y - 5;
        sprite.texInfo.u = 38;
        sprite.texInfo.v = 144;
        sprite.size.x = 5;
        sprite.size.y = 5;

        gpu.chain(spriteFrag);
    }

    { // left
        for (int i = 0; i < 5; ++i) {
            drawLine(
                primBuffer,
                gpu,
                position.x - 5 + i,
                position.y,
                position.x - 5 + i,
                position.y + size.y,
                dbBorderColors[i]);
        }
    }

    { // right
        for (int i = 0; i < 5; ++i) {
            drawLine(
                primBuffer,
                gpu,
                position.x + size.x + 4 - i,
                position.y,
                position.x + size.x + 4 - i,
                position.y + size.y,
                dbBorderColors[i]);
        }
    }

    { // bottom left
        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = borderTexture.clut;

        sprite.position.x = position.x - 5;
        sprite.position.y = position.y + size.y;
        sprite.texInfo.u = 32;
        sprite.texInfo.v = 150;
        sprite.size.x = 5;
        sprite.size.y = 5;

        gpu.chain(spriteFrag);
    }

    { // bottom
        for (int i = 0; i < 5; ++i) {
            drawLine(
                primBuffer,
                gpu,
                position.x,
                position.y + size.y + 4 - i,
                position.x + size.x,
                position.y + size.y + 4 - i,
                dbBorderColors[i]);
        }
    }

    { // bottom right
        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = borderTexture.clut;

        sprite.position.x = position.x + size.x;
        sprite.position.y = position.y + size.y;
        sprite.texInfo.u = 38;
        sprite.texInfo.v = 150;
        sprite.size.x = 5;
        sprite.size.y = 5;

        gpu.chain(spriteFrag);
    }
}

void DialogueBox::drawText(
    Renderer& renderer,
    const Font& font,
    const TextureInfo& fontAtlasTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    auto currTextColor = getRainbowColor(7);
    const psyqo::Vertex glyphPos{{
        .x = static_cast<int16_t>(position.x + textOffset.x),
        .y = static_cast<int16_t>(position.y + textOffset.y),
    }};

    int numCharsShown = 0;
    allTextShown = true;

    int lineNum = 0;
    int lineX = 0; // current line X offset

    const auto lineSpacing = font.lineSpacing;
    const auto ascenderPx = font.lineSpacing;
    const auto& glyphs = font.glyphs;
    const auto fontClut = fontAtlasTexture.clut;

    bool wavyText = false;
    bool rainbowText = false;
    for (int i = 0; i < textString.size(); ++i) {
        const auto c = textString[i];

        // newline
        if (c == '\n') {
            ++lineNum;
            lineX = 0;
            continue;
        }

        // text control
        if (c == 1) { // white
            currTextColor = getRainbowColor(7);
            continue;
        } else if (c == 2) { // yellow
            currTextColor = getRainbowColor(0);
            continue;
        } else if (c == 3) { // wavy
            wavyText = !wavyText;
            continue;
        } else if (c == 4) { // rainbow
            rainbowText = !rainbowText;
            continue;
        } else if (c == 5) {
            currTextColor = getRainbowColor(4);
            continue;
        }

        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = fontClut;

        auto& glyphInfo = glyphs[(int)c];
        sprite.position.x = glyphPos.x + lineX + glyphInfo.bearing.x;
        sprite.position.y = glyphPos.y + (ascenderPx - glyphInfo.bearing.y) + lineNum * lineSpacing;
        sprite.texInfo.u = glyphInfo.uv0.x;
        sprite.texInfo.v = glyphInfo.uv0.y;
        sprite.size.x = glyphInfo.size.x;
        sprite.size.y = glyphInfo.size.y;

        if (wavyText) {
            sprite.position.y += waveOffsets[(numCharsShown + wavePhase) % (waveMax + 1)];
        }

        if (rainbowText) {
            sprite.setColor(getRainbowColor(numCharsShown));
        } else {
            sprite.setColor(currTextColor);
        }

        gpu.chain(spriteFrag);

        lineX += glyphInfo.advance;

        ++numCharsShown;
        if (numCharsShown > numOfCharsToShow) {
            allTextShown = false;
            break;
        }
    }
}

void DialogueBox::drawMoreTextIndicator(Renderer& renderer, const TextureInfo& borderTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    if (allTextShown) { // more text
        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite8x8>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = borderTexture.clut;

        sprite.position.x = position.x + size.x - moreTextOffset.x;
        sprite.position.y = position.y + size.y - moreTextOffset.y + moreTextBouncer.getOffset();
        sprite.texInfo.u = 48;
        sprite.texInfo.v = 144;

        gpu.chain(spriteFrag);
    }
}

void DialogueBox::drawLine(
    Renderer::PrimBufferAllocatorType& primBuffer,
    psyqo::GPU& gpu,
    int16_t aX,
    int16_t aY,
    int16_t bX,
    int16_t bY,
    const psyqo::Color& color)
{
    auto& lineFrag = primBuffer.allocateFragment<psyqo::Prim::Line>();
    auto& line = lineFrag.primitive;
    line.pointA.x = aX;
    line.pointA.y = aY;
    line.pointB.x = bX;
    line.pointB.y = bY;
    line.setColor(color);

    gpu.chain(lineFrag);
}

void DialogueBox::setText(const char* text, bool displayImmediately)
{
    textString = text;
    if (displayImmediately) {
        numOfCharsToShow = textString.size();
    } else {
        numOfCharsToShow = 0;
    }
    wantClose = false;
    letterIncrementTimer.reset();
}
