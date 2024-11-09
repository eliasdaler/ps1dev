#pragma once

namespace psyqo
{
struct GPU;
struct SimplePad;
};

class Renderer;
struct TextureInfo;

class DialogueBox {
public:
    void handleInput(const psyqo::SimplePad& pad);
    void update();
    void draw(Renderer& renderer, psyqo::GPU& gpu, const TextureInfo& fontAtlasTexture);

private:
    int numOfCharsToShow{0};
};
