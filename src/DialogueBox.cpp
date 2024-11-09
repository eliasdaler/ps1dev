#include "DialogueBox.h"

#include "Renderer.h"

#include <psyqo/primitives/lines.hh>
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/primitives/sprites.hh>
#include <psyqo/simplepad.hh>

#include <EASTL/fixed_string.h>

void DialogueBox::handleInput(const psyqo::SimplePad& pad)
{
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Cross)) {
        numOfCharsToShow = 0;
    }
}

void DialogueBox::update()
{}

void DialogueBox::draw(Renderer& renderer, psyqo::GPU& gpu, const TextureInfo& fontAtlasTexture)
{
    const auto parity = gpu.getParity();
    auto& ot = renderer.ots[parity];
    auto& primBuffer = renderer.primBuffers[parity];

    static constexpr auto dbX = 60;
    static constexpr auto dbY = 140;
    static constexpr auto dbW = 200;
    static constexpr auto dbH = 64;

    { // bg rect
        auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
        auto& rect = rectFrag.primitive;
        rect.position.x = dbX;
        rect.position.y = dbY;
        rect.size.x = dbW;
        rect.size.y = dbH;

        static const auto black = psyqo::Color{{0, 0, 0}};
        rect.setColor(black);
        rect.setSemiTrans();
        gpu.chain(rectFrag);
    }

    static const eastl::fixed_string<char, 256> textString{"AAAAAAAAAAAA\nBB \2AA \1BB"};

    struct GlyphInfo {
        int u;
        int v;
    };

    static GlyphInfo glyphInfos[255];
    glyphInfos['A'] = GlyphInfo{0, 144};
    glyphInfos['B'] = GlyphInfo{16, 144};

    static const auto white = psyqo::Color{{128, 128, 128}};
    static const auto red = psyqo::Color{{255, 0, 0}};

    static constexpr auto textOffsetX = 0;
    static constexpr auto textOffsetY = 0;
    static const auto glyphW = 16;
    static const auto glyphH = 16;
    static const auto lineH = 16;

    auto currColor = white;
    auto glyphX = dbX + textOffsetX;
    auto glyphY = dbY + textOffsetY;

    static int timer = 0;
    static bool allTextShown = true;

    { // sprites
        // set tpage
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr = fontAtlasTexture.tpage;
        gpu.chain(tpage);

        if (timer == 2) {
            timer = 0;
            ++numOfCharsToShow;
        } else {
            ++timer;
        }

        int numCharsShown = 0;
        allTextShown = true;
        for (int i = 0; i < textString.size(); ++i) {
            auto c = textString[i];

            if (c == '\n') {
                glyphX = dbX + textOffsetX;
                glyphY += lineH;
                continue;
            }

            if (c == ' ') {
                glyphX += glyphW; // TODO: kern here
                continue;
            }

            if (c == 1) {
                currColor = white;
                continue;
            } else if (c == 2) {
                currColor = red;
                continue;
            }

            auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite16x16>();
            auto& sprite = spriteFrag.primitive;
            sprite.texInfo.clut = fontAtlasTexture.clut;

            sprite.position.x = glyphX;
            sprite.position.y = glyphY;
            auto& glyphInfo = glyphInfos[(int)c];
            sprite.texInfo.u = glyphInfo.u;
            sprite.texInfo.v = glyphInfo.v;

            sprite.setColor(currColor);
            gpu.chain(spriteFrag);

            glyphX += glyphW; // TODO: kern here
            ++numCharsShown;
            if (numCharsShown > numOfCharsToShow) {
                allTextShown = false;
                break;
            }
        }

        { // top left
            auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
            auto& sprite = spriteFrag.primitive;
            sprite.texInfo.clut = fontAtlasTexture.clut;

            sprite.position.x = dbX - 5;
            sprite.position.y = dbY - 5;
            sprite.texInfo.u = 32;
            sprite.texInfo.v = 144;
            sprite.size.x = 5;
            sprite.size.y = 5;

            sprite.setColor(white);
            gpu.chain(spriteFrag);
        }

        auto drawDBLine = [&](int aX, int aY, int bX, int bY, const psyqo::Color& color) {
            static const auto dbBlackColor = psyqo::Color{{24, 30, 37}};
            auto& lineFrag = primBuffer.allocateFragment<psyqo::Prim::Line>();
            auto& line = lineFrag.primitive;
            line.pointA.x = aX;
            line.pointA.y = aY;
            line.pointB.x = bX;
            line.pointB.y = bY;
            line.setColor(color);

            gpu.chain(lineFrag);
        };

        static const psyqo::Color dbBorderColors[5] =
            {psyqo::Color{{24, 30, 37}}, // black
             psyqo::Color{{116, 63, 57}}, // brown
             psyqo::Color{{254, 174, 52}}, // orange
             psyqo::Color{{254, 231, 97}}, // yellow
             psyqo::Color{{24, 30, 37}}}; // black

        { // top
            for (int i = 0; i < 5; ++i) {
                drawDBLine(dbX, dbY - 5 + i, dbX + dbW, dbY - 5 + i, dbBorderColors[i]);
            }
        }

        { // top right
            auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
            auto& sprite = spriteFrag.primitive;
            sprite.texInfo.clut = fontAtlasTexture.clut;

            sprite.position.x = dbX + dbW;
            sprite.position.y = dbY - 5;
            sprite.texInfo.u = 38;
            sprite.texInfo.v = 144;
            sprite.size.x = 5;
            sprite.size.y = 5;

            sprite.setColor(white);
            gpu.chain(spriteFrag);
        }

        { // left
            for (int i = 0; i < 5; ++i) {
                drawDBLine(dbX - 5 + i, dbY, dbX - 5 + i, dbY + dbH, dbBorderColors[i]);
            }
        }

        { // right
            for (int i = 0; i < 5; ++i) {
                drawDBLine(dbX + dbW + 4 - i, dbY, dbX + dbW + 4 - i, dbY + dbH, dbBorderColors[i]);
            }
        }

        { // bottom left
            auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
            auto& sprite = spriteFrag.primitive;
            sprite.texInfo.clut = fontAtlasTexture.clut;

            sprite.position.x = dbX - 5;
            sprite.position.y = dbY + dbH;
            sprite.texInfo.u = 32;
            sprite.texInfo.v = 150;
            sprite.size.x = 5;
            sprite.size.y = 5;

            sprite.setColor(white);
            gpu.chain(spriteFrag);
        }

        { // bottom
            for (int i = 0; i < 5; ++i) {
                drawDBLine(dbX, dbY + dbH + 4 - i, dbX + dbW, dbY + dbH + 4 - i, dbBorderColors[i]);
            }
        }

        { // bottom right
            auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite>();
            auto& sprite = spriteFrag.primitive;
            sprite.texInfo.clut = fontAtlasTexture.clut;

            sprite.position.x = dbX + dbW;
            sprite.position.y = dbY + dbH;
            sprite.texInfo.u = 38;
            sprite.texInfo.v = 150;
            sprite.size.x = 5;
            sprite.size.y = 5;

            sprite.setColor(white);
            gpu.chain(spriteFrag);
        }

        static bool moveDown = false;
        static int moveOffset = 0;
        static int moveOffsetMax = 1;
        static int timer2 = 0;
        if (timer2 == 3) {
            timer2 = 0;
            if (moveDown) {
                ++moveOffset;
            } else {
                --moveOffset;
            }
        } else {
            ++timer2;
        }
        if (moveOffset > moveOffsetMax) {
            moveDown = false;
        } else if (moveOffset < -moveOffsetMax) {
            moveDown = true;
        }

        if (allTextShown) { // more text
            auto& spriteFrag = primBuffer.allocateFragment<psyqo::Prim::Sprite8x8>();
            auto& sprite = spriteFrag.primitive;
            sprite.texInfo.clut = fontAtlasTexture.clut;

            sprite.position.x = dbX + dbW - 14;
            sprite.position.y = dbY + dbH - 10 + moveOffset;
            sprite.texInfo.u = 48;
            sprite.texInfo.v = 144;

            sprite.setColor(white);
            gpu.chain(spriteFrag);
        }
    }
}
