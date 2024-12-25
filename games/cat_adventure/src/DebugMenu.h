#pragma once

#include <EASTL/functional.h>
#include <EASTL/vector.h>

struct Renderer;
struct TextureInfo;
struct Font;
struct PadManager;

struct DebugMenu {
    void init(
        const Font& font,
        const TextureInfo& uiElementsTexture,
        const TextureInfo& fontAtlasTexture);
    void processInput(const PadManager& pad);
    void draw(Renderer& renderer);

    struct MenuItem {
        const char* text;
        eastl::function<void()> callback;
    };
    eastl::vector<MenuItem> menuItems;

    const Font* fontPtr;
    const TextureInfo* fontAtlasTexturePtr;
    const TextureInfo* uiElementsTexturePtr;

    bool open{false};
    int selectedItemIdx{0};
};
