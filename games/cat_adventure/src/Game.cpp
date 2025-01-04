#include "Game.h"

#include <common/syscalls/syscalls.h>

#include "StringHashes.h"

namespace
{
const int startLevel = 0;

const char* getLevelModelPath(int levelId)
{
    switch (levelId) {
    case 0:
        return "LEVEL.BIN;1";
    case 1:
        return "LEVEL2.BIN;1";
    }
    return "";
}
}

Game::Game() :
    cd(*this),
    renderer(gpu()),
    gameplayScene(*this),
    loadingScene(*this),
    songPlayer(gpu(), soundPlayer)
{}

void Game::prepare()
{
    initStringHashes();

    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::NTSC)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);

    cd.init();

    soundPlayer.init();
    renderer.init();
}

namespace
{
psyqo::Coroutine<> loadCoroutine(Game& game)
{
    auto& resourceCache = game.resourceCache;

    psyqo::Coroutine<>::Awaiter awaiter = game.gameLoadCoroutine.awaiter();

    if (game.firstLoad) { // core
        game.cd.loadTIM("FONT.TIM;1", game.fontTexture);
        co_await awaiter;

        game.cd.loadFont("FONT.FNT;1", game.font);
        co_await awaiter;

        game.debugMenu.init(game.font, game.fontTexture, game.fontTexture);
    }

    if (game.firstLoad) { // music and sounds
        game.cd.loadMIDI("SONG.MID;1", game.midi);
        co_await awaiter;

        game.cd.loadInstruments("INST.VAB;1", game.vab);
        co_await awaiter;

        game.cd.loadRawPCM("SMPL.PCM;1", 0x1010);
        co_await awaiter;

        game.step1Sound = 0x3300;
        game.cd.loadSound("STEP1.VAG;1", game.step1Sound);
        co_await awaiter;

        game.step2Sound = 0x3F00;
        game.cd.loadSound("STEP2.VAG;1", game.step2Sound);
        co_await awaiter;
    }

    if (!game.firstLoad) {
        for (const auto& filename : game.level.usedTextures) {
            resourceCache.derefResource<TextureInfo>(filename);
        }

        for (const auto& filename : game.level.usedModels) {
            resourceCache.derefResource<Model>(filename);
        }
    }

    game.level.id = game.levelToLoad;
    if (game.levelToLoad == 0) {
        game.cd.loadLevel(LEVEL1_LEVEL_HASH.getStr(), game.level);
        co_await awaiter;
    } else {
        // TODO: load from .lvl file
        game.level.collisionBoxes.clear();
        game.level.usedTextures = {
            CATO_TEXTURE_HASH,
        };
        game.level.usedModels = {
            CATO_MODEL_HASH,
            LEVEL2_MODEL_HASH,
        };
    }

    { // clean up unused resources
        for (const auto& filename : game.level.usedTextures) {
            if (resourceCache.resourceLoaded<TextureInfo>(filename)) {
                resourceCache.refResource<TextureInfo>(filename);
            }
        }

        for (const auto& filename : game.level.usedModels) {
            if (resourceCache.resourceLoaded<Model>(filename)) {
                resourceCache.refResource<Model>(filename);
            }
        }

        resourceCache.removeUnusedResources<TextureInfo>();
        resourceCache.removeUnusedResources<Model>();
    }

    { // load new textures
        for (const auto& filename : game.level.usedTextures) {
            if (!resourceCache.resourceLoaded<TextureInfo>(filename)) {
                const auto& filenameStr = filename.getStr();
                ramsyscall_printf("[!] Loading texture '%s'\n", filenameStr);

                TextureInfo texture;
                game.cd.loadTIM(filenameStr, texture);
                co_await awaiter;

                resourceCache.putResource<TextureInfo>(filename, eastl::move(texture));
            }
        }
    }

    { // load new models
        for (const auto& filename : game.level.usedModels) {
            if (!resourceCache.resourceLoaded<Model>(filename)) {
                const auto& filenameStr = filename.getStr();
                ramsyscall_printf("[!] Loading model '%s'\n", filenameStr);

                Model model;
                game.cd.loadFastModel(filenameStr, model);
                co_await awaiter;

                resourceCache.putResource<Model>(filename, eastl::move(model));
            }
        }
    }

    if (game.firstLoad) {
        game.cd.loadAnimations("HUMAN.ANM;1", game.humanAnimations);
        co_await awaiter;

        game.cd.loadAnimations("CATO.ANM;1", game.catAnimations);
        co_await awaiter;
    }

    game.popScene(); // pop loading scene
    if (game.firstLoad) {
        game.pushScene(&game.gameplayScene);
        game.firstLoad = false;
    }

    ramsyscall_printf("-----\n");
}
}

void Game::createScene()
{
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    pad.init();

    loadLevel(startLevel);
}

void Game::loadLevel(int levelId)
{
    ramsyscall_printf("-----\nLoading level: %d\n", levelId);

    levelToLoad = levelId;

    pushScene(&loadingScene);

    // load resources
    gameLoadCoroutine = loadCoroutine(*this);
    gameLoadCoroutine.resume();
}
void Game::handleDeltas()
{
    auto currVSyncs = gpu().getFrameCount();
    currNow = gpu().now();
    currVSyncs = gpu().getFrameCount();

    if (currNow > prevNow) {
        frameDtMcs = currNow - prevNow;
        frameDt = psyqo::FixedPoint<>(frameDtMcs / 1000, 0) / psyqo::FixedPoint<>(1000.0);
    } else {
        // overflow
        frameDtMcs = 0;
        frameDt = 0.0;
    }
    if (currVSyncs > prevVSyncs) {
        vSyncDiff = currVSyncs - prevVSyncs;
    } else {
        // overflow
        vSyncDiff = 0;
    }

    if (vSyncDiff == 0 || frameDtMcs == 0) { // skip "overflow" frame
        return;
    }
}

void Game::onFrameEnd()
{
    prevVSyncs = currVSyncs;
    prevNow = currNow;
}
