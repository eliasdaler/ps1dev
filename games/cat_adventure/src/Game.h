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
#include <Graphics/Model.h>
#include <Graphics/Renderer.h>
#include <Graphics/SkeletalAnimation.h>
#include <Graphics/TextureInfo.h>
#include <Graphics/TimFile.h>

#include "GameplayScene.h"
#include "LoadingScene.h"

#include "CDLoader.h"
#include "Level.h"
#include "ResourceCache.h"

class Game : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    Game();

    void loadLevel(int levelId);

    CDLoader cd;
    psyqo::Font<> romFont;
    psyqo::Trig<> trig;

    psyqo::Coroutine<> gameLoadCoroutine;

    PadManager pad;

    TextureInfo fontTexture;
    Font font;

    Model levelModel;
    Model level2Model;
    Model catoModel;
    Model humanModel;
    Model carModel;

    eastl::vector<SkeletalAnimation> animations;

    Level level;
    int levelToLoad;

    Renderer renderer;

    SoundPlayer soundPlayer;
    SongPlayer songPlayer;

    uint32_t step1Sound{0};
    uint32_t step2Sound{0};

    MidiFile midi;
    VabFile vab;

    GameplayScene gameplayScene;
    LoadingScene loadingScene;

    bool firstLoad{true};

    DebugMenu debugMenu;

    ResourceCache resourceCache;
};
