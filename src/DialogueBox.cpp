#include "DialogueBox.h"

#include "Renderer.h"

#include <psyqo/primitives/lines.hh>
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/primitives/sprites.hh>
#include <psyqo/simplepad.hh>

DialogueBox::DialogueBox()
{
    glyphInfos['A'] = GlyphInfo{0, 144};
    glyphInfos['B'] = GlyphInfo{16, 144};
}

void DialogueBox::handleInput(const psyqo::SimplePad& pad)
{
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Cross)) {
        if (allTextShown) {
            numOfCharsToShow = 0;
        }
        // TODO: if not shown and pressed - show all text
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
}

void DialogueBox::draw(Renderer& renderer, const TextureInfo& fontAtlasTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    // set tpage
    auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
    tpage.primitive.attr = fontAtlasTexture.tpage;
    gpu.chain(tpage);

    drawBG(renderer, fontAtlasTexture);
    drawText(renderer, fontAtlasTexture);
    drawMoreTextIndicator(renderer, fontAtlasTexture);
}

void DialogueBox::drawBG(Renderer& renderer, const TextureInfo& fontAtlasTexture)
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

    static const psyqo::Color dbBorderColors[5] =
        {psyqo::Color{{24, 30, 37}}, // black
         psyqo::Color{{116, 63, 57}}, // brown
         psyqo::Color{{254, 174, 52}}, // orange
         psyqo::Color{{254, 231, 97}}, // yellow
         psyqo::Color{{24, 30, 37}}}; // black

    { // top left
        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = fontAtlasTexture.clut;

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
        sprite.texInfo.clut = fontAtlasTexture.clut;

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
        sprite.texInfo.clut = fontAtlasTexture.clut;

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
        sprite.texInfo.clut = fontAtlasTexture.clut;

        sprite.position.x = position.x + size.x;
        sprite.position.y = position.y + size.y;
        sprite.texInfo.u = 38;
        sprite.texInfo.v = 150;
        sprite.size.x = 5;
        sprite.size.y = 5;

        gpu.chain(spriteFrag);
    }
}

void DialogueBox::drawText(Renderer& renderer, const TextureInfo& fontAtlasTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    static const auto textWhite = psyqo::Color{{128, 128, 128}};
    static const auto textRed = psyqo::Color{{255, 0, 0}};

    static const auto lineH = 16;

    auto currTextColor = textWhite;
    psyqo::Vertex glyphPos{{
        .x = static_cast<int16_t>(position.x + textOffset.x),
        .y = static_cast<int16_t>(position.y + textOffset.y),
    }};

    int numCharsShown = 0;
    allTextShown = true;

    for (int i = 0; i < textString.size(); ++i) {
        auto c = textString[i];

        // newline
        if (c == '\n') {
            glyphPos.x = position.x + textOffset.x;
            glyphPos.y += lineH;
            continue;
        }

        if (c == ' ') {
            glyphPos.x += glyphSize.x; // TODO: kern here
            continue;
        }

        // text color control
        if (c == 1) {
            currTextColor = textWhite;
            continue;
        } else if (c == 2) {
            currTextColor = textRed;
            continue;
        }

        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite16x16>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = fontAtlasTexture.clut;

        sprite.position = glyphPos;
        auto& glyphInfo = glyphInfos[(int)c];
        sprite.texInfo.u = glyphInfo.u;
        sprite.texInfo.v = glyphInfo.v;

        sprite.setColor(currTextColor);
        gpu.chain(spriteFrag);

        glyphPos.x += glyphSize.w; // TODO: kern here

        ++numCharsShown;
        if (numCharsShown > numOfCharsToShow) {
            allTextShown = false;
            break;
        }
    }
}

void DialogueBox::drawMoreTextIndicator(Renderer& renderer, const TextureInfo& fontAtlasTexture)
{
    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    if (allTextShown) { // more text
        auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite8x8>();
        auto& sprite = spriteFrag.primitive;
        sprite.texInfo.clut = fontAtlasTexture.clut;

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
