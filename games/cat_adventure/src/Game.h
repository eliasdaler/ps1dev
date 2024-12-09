#pragma once

#include <psyqo-paths/cdrom-loader.hh>
#include <psyqo/application.hh>
#include <psyqo/cdrom-device.hh>
#include <psyqo/coroutine.hh>
#include <psyqo/font.hh>
#include <psyqo/simplepad.hh>
#include <psyqo/trigonometry.hh>

#include "Font.h"
#include "MidiFile.h"
#include "Model.h"
#include "Renderer.h"
#include "SkeletalAnimation.h"
#include "SongPlayer.h"
#include "SoundPlayer.h"
#include "TextureInfo.h"
#include "TimFile.h"
#include "VabFile.h"

class Game : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    Game();

    void loadTIM(eastl::string_view filename, TextureInfo& textureInfo);
    void loadFont(eastl::string_view filename, Font& font);
    void loadModel(eastl::string_view filename, Model& model);
    void loadSound(eastl::string_view filename, Sound& sound);
    void loadMIDI(eastl::string_view filename, MidiFile& midi);
    void loadInstruments(eastl::string_view filename, VabFile& vab);
    void loadRawPCM(eastl::string_view filename, uint32_t spuUploadAddr);
    void loadAnimations(eastl::string_view filename, eastl::vector<SkeletalAnimation>& animations);

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
    Model carModel;

    TextureInfo bricksTexture;
    TextureInfo catoTexture;
    TextureInfo carTexture;

    eastl::vector<SkeletalAnimation> animations;

    TextureInfo fontTexture;
    Font font;

    int levelId{0};

    Renderer renderer;

    SoundPlayer soundPlayer;
    SongPlayer songPlayer;

    MidiFile midi;
    VabFile vab;
};
