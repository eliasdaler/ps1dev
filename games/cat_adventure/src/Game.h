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

    CDLoader cd;
    psyqo::Coroutine<> gameLoadCoroutine;
    ResourceCache resourceCache;
    bool firstLoad{true};

    psyqo::Font<> romFont;
    psyqo::Trig<> trig;

    PadManager pad;

    Renderer renderer;

    Level level;
    int levelToLoad;

    // scenes
    GameplayScene gameplayScene;
    LoadingScene loadingScene;

    // data resources
    TextureInfo fontTexture;
    Font font;
    eastl::vector<SkeletalAnimation> animations;

    // audio
    MidiFile midi;
    VabFile vab;
    SoundPlayer soundPlayer;
    SongPlayer songPlayer;
    uint32_t step1Sound{0};
    uint32_t step2Sound{0};

    DebugMenu debugMenu;
};
