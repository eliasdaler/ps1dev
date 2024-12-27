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
    int processInput(const PadManager& pad);
    void draw(Renderer& renderer);

    struct MenuItem {
        const char* text;
        eastl::function<void()> callback;

        bool checkbox{false};
        bool checkboxOn{false};
        bool* valuePtr{nullptr}; // value to change on checkbox modify
    };

    static constexpr auto DUMP_DEBUG_INFO_ITEM_ID = 0;
    static constexpr auto COLLISION_ITEM_ID = 1;
    static constexpr auto FOLLOW_CAMERA_ITEM_ID = 2;
    static constexpr auto MUTE_MUSIC_ITEM_ID = 3;
    static constexpr auto DRAW_COLLISION_ITEM_ID = 4;

    eastl::vector<MenuItem> menuItems;

    const Font* fontPtr;
    const TextureInfo* fontAtlasTexturePtr;
    const TextureInfo* uiElementsTexturePtr;

    bool open{false};
    int selectedItemIdx{0};
};
