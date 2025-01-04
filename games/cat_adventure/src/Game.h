#pragma once

#include <psyqo/application.hh>
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
#include <Graphics/Renderer.h>
#include <Graphics/SkeletalAnimation.h>
#include <Graphics/TextureInfo.h>

#include <GameplayScene.h>
#include <LoadingScene.h>

#include <CDLoader.h>
#include <Level.h>
#include <ResourceCache.h>

class Game : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    Game();

    void loadLevel(int levelId);

    void handleDeltas();
    void onFrameEnd();

    CDLoader cd;
    psyqo::Coroutine<> gameLoadCoroutine;
    ResourceCache resourceCache;
    bool firstLoad{true};

    psyqo::Font<> romFont;
    psyqo::Trig<> trig;

    PadManager pad;
    Renderer renderer;

    Level level;

    // scenes
    GameplayScene gameplayScene;
    LoadingScene loadingScene;

    // data resources
    TextureInfo fontTexture;
    Font font;

    eastl::vector<SkeletalAnimation> humanAnimations;
    eastl::vector<SkeletalAnimation> catAnimations;

    // audio
    MidiFile midi;
    VabFile vab;
    SoundPlayer soundPlayer;
    SongPlayer songPlayer;
    uint32_t step1Sound{0};
    uint32_t step2Sound{0};

    DebugMenu debugMenu;

    // set by loadLevel
    int levelToLoad;

    FastModel catoModelFast;
    FastModel humanModelFast;
    FastModel levelModelFast;
    FastModel level2ModelFast;

    uint32_t frameDtMcs{0}; // dt in microseconds
    psyqo::FixedPoint<> frameDt{0}; // dt in seconds
    uint32_t vSyncDiff{0}; // number of vsync between Application::frame invocations

    uint32_t prevNow{0};
    uint32_t currNow{0};

    uint32_t prevVSyncs{0};
    uint32_t currVSyncs{0};
};

extern Game g_game;
