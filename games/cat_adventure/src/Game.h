#pragma once

#include <psyqo-paths/cdrom-loader.hh>
#include <psyqo/application.hh>
#include <psyqo/cdrom-device.hh>
#include <psyqo/coroutine.hh>
#include <psyqo/font.hh>
#include <psyqo/trigonometry.hh>

#include <Audio/MidiFile.h>
#include <Audio/SongPlayer.h>
#include <Audio/SoundPlayer.h>
#include <Audio/VabFile.h>
#include <Core/PadManager.h>
#include <Dev/DebugMenu.h>
#include <Graphics/Font.h>
#include <Graphics/Model.h>
#include <Graphics/Renderer.h>
#include <Graphics/SkeletalAnimation.h>
#include <Graphics/TextureInfo.h>
#include <Graphics/TimFile.h>

#include "GameplayScene.h"
#include "LoadingScene.h"

class Game : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    Game();

    void loadTIM(eastl::string_view filename, TextureInfo& textureInfo);
    void loadFont(eastl::string_view filename, Font& font);
    void loadModel(eastl::string_view filename, Model& model);
    void loadSound(eastl::string_view filename, uint32_t spuUploadAddr);
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
    psyqo::Coroutine<> gameLoadCoroutine;

    PadManager pad;

    Model levelModel;
    Model level2Model;
    Model catoModel;
    Model humanModel;
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

    uint32_t step1Sound{0};
    uint32_t step2Sound{0};

    MidiFile midi;
    VabFile vab;

    GameplayScene gameplayScene;
    LoadingScene loadingScene;

    DebugMenu debugMenu;
};
