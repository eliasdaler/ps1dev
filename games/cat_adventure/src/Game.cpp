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
    songPlayer(gpu(), soundPlayer),
    gameplayScene(*this),
    loadingScene(*this)
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

    const auto texturesToLoad = [](int levelId) -> eastl::vector<StringViewWithHash> {
        switch (levelId) {
        case 0:
            return {
                StringViewWithHash{"BRICKS.TIM;1"},
                StringViewWithHash{"CATO.TIM;1"},
            };
        case 1:
            return {
                StringViewWithHash{"CATO.TIM;1"},
                StringViewWithHash{"CAR.TIM;1"},
            };
        default:
            return {};
        }
    }(game.levelToLoad);

    { // textures
        if (!game.firstLoad) {
            for (const auto& filename : game.level.usedTextures) {
                resourceCache.derefResource<TextureInfo>(filename);
            }
        }

        for (const auto& filename : texturesToLoad) {
            if (resourceCache.resourceLoaded<TextureInfo>(filename.hash)) {
                resourceCache.refResource<TextureInfo>(filename.hash);
            }
        }

        resourceCache.removeUnusedResources<TextureInfo>();

        game.level.usedTextures.clear();
        for (const auto& filename : texturesToLoad) {
            if (!resourceCache.resourceLoaded<TextureInfo>(filename.hash)) {
                TextureInfo texture;
                ramsyscall_printf("[!] Loading texture '%s'\n", filename.str);
                game.cd.loadTIM(filename.str, texture);
                co_await awaiter;

                resourceCache.putResource<TextureInfo>(filename.hash, eastl::move(texture));
            }
            game.level.usedTextures.push_back(filename.hash);
        }
    }

    const auto modelsToLoad = [](int levelId) -> eastl::vector<StringViewWithHash> {
        switch (levelId) {
        case 0:
            return {
                StringViewWithHash{"HUMAN.BIN;1"},
                StringViewWithHash{"LEVEL.BIN;1"},
                StringViewWithHash{"CATO.BIN;1"},
            };
        case 1:
            return {
                StringViewWithHash{"HUMAN.BIN;1"},
                StringViewWithHash{"LEVEL2.BIN;1"},
                StringViewWithHash{"CAR.BIN;1"},
            };
        default:
            return {};
        }
    }(game.levelToLoad);

    { // models
        if (!game.firstLoad) {
            for (const auto& filename : game.level.usedModels) {
                resourceCache.derefResource<Model>(filename);
            }
        }

        for (const auto& filename : modelsToLoad) {
            if (resourceCache.resourceLoaded<Model>(filename.hash)) {
                resourceCache.refResource<Model>(filename.hash);
            }
        }

        resourceCache.removeUnusedResources<Model>();

        game.level.usedModels.clear();
        for (const auto& filename : modelsToLoad) {
            if (!resourceCache.resourceLoaded<Model>(filename.hash)) {
                Model model;
                ramsyscall_printf("[!] Loading model '%s'\n", filename.str);
                game.cd.loadModel(filename.str, model);
                co_await awaiter;

                resourceCache.putResource<Model>(filename.hash, eastl::move(model));
            }

            game.level.usedModels.push_back(filename.hash);
        }
    }

    if (game.firstLoad) {
        game.cd.loadAnimations("HUMAN.ANM;1", game.animations);
        co_await awaiter;
    }

    game.level.id = game.levelToLoad;

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
