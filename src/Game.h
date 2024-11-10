#pragma once

#include <psyqo-paths/cdrom-loader.hh>
#include <psyqo/application.hh>
#include <psyqo/cdrom-device.hh>
#include <psyqo/coroutine.hh>
#include <psyqo/font.hh>
#include <psyqo/simplepad.hh>
#include <psyqo/trigonometry.hh>

#include "Font.h"
#include "Model.h"
#include "TextureInfo.h"
#include "TimFile.h"

class Game : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    void loadTIM(eastl::string_view filename, TextureInfo& textureInfo);
    void loadFont(eastl::string_view filename, Font& font);
    void loadModel(eastl::string_view filename, Model& model);

    [[nodiscard]] TextureInfo uploadTIM(const TimFile& tim);

    psyqo::Font<> romFont;
    psyqo::Trig<> trig;

    psyqo::CDRomDevice cdrom;
    psyqo::ISO9660Parser isoParser{&cdrom};
    psyqo::paths::CDRomLoader cdromLoader;
    eastl::vector<std::uint8_t> cdReadBuffer;
    psyqo::Coroutine<> cdLoadCoroutine;

    psyqo::SimplePad pad;

    Model levelModel;
    Model catoModel;

    TextureInfo bricksTexture;
    TextureInfo catoTexture;
    TextureInfo fontTexture;
    Font font;
};
