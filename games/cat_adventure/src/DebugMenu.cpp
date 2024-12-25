#include "DebugMenu.h"

#include "Common.h"
#include "PadManager.h"
#include "Renderer.h"
#include "TextLabel.h"

#include <common/syscalls/syscalls.h>
#include <psyqo/primitives/rectangles.hh>

void DebugMenu::init(
    const Font& font,
    const TextureInfo& uiElementsTexture,
    const TextureInfo& fontAtlasTexture)
{
    fontPtr = &font;
    uiElementsTexturePtr = &uiElementsTexture;
    fontAtlasTexturePtr = &fontAtlasTexture;

    menuItems = {
        MenuItem{
            .text = "Change level", .callback = []() { ramsyscall_printf("Change level!\n"); }},
        MenuItem{
            .text = "Dump debug info",
            .callback = []() { ramsyscall_printf("Dump debug info\n"); },
        },
        MenuItem{
            .text = "Funky mode",
        },
    };

    open = true;
}

void DebugMenu::processInput(const PadManager& pad)
{
    if (!open) {
        // TODO
        return;
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Button::Down)) {
        ++selectedItemIdx;
        if (selectedItemIdx >= menuItems.size()) {
            selectedItemIdx = 0;
        }
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Button::Up)) {
        --selectedItemIdx;
        if (selectedItemIdx < 0) {
            selectedItemIdx = menuItems.size() - 1;
        }
    }

    if (pad.wasButtonJustPressed(psyqo::SimplePad::Button::Cross)) {
        const auto& menuItem = menuItems[selectedItemIdx];
        if (menuItem.callback) {
            menuItem.callback();
        }
    }
}

void DebugMenu::draw(Renderer& renderer)
{
    if (!open) {
        return;
    }

    static const auto normalItemColor = psyqo::Color{.r = 255, .g = 255, .b = 255};
    static const auto selectedItemColor = psyqo::Color{.r = 255, .g = 255, .b = 0};

    auto& primBuffer = renderer.getPrimBuffer();
    auto& gpu = renderer.getGPU();

    { // bg
        auto& rectFrag = primBuffer.allocateFragment<psyqo::Prim::Rectangle>();
        auto& rect = rectFrag.primitive;
        rect.position = {};
        rect.size.x = SCREEN_WIDTH;
        rect.size.y = SCREEN_HEIGHT;

        static const auto black = psyqo::Color{.r = 0, .g = 0, .b = 0};
        rect.setColor(black);
        rect.setSemiTrans();
        gpu.chain(rectFrag);
    }

    {
        auto& tpage = primBuffer.allocateFragment<psyqo::Prim::TPage>();
        tpage.primitive.attr = fontAtlasTexturePtr->tpage;
        gpu.chain(tpage);
    }

    psyqo::Vertex textPos{.x = 32, .y = 32};
    for (int i = 0; i < menuItems.size(); ++i) {
        bool selected = (i == selectedItemIdx);

        const auto& menuItem = menuItems[i];
        ui::drawTextLabel(
            textPos,
            menuItem.text,
            selected ? selectedItemColor : normalItemColor,
            renderer,
            *fontPtr,
            *fontAtlasTexturePtr,
            false);

        textPos.y += 16;
    }
}
