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
    renderer(gpu()), songPlayer(gpu(), soundPlayer), gameplayScene(*this), loadingScene(*this)
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
    cdrom.prepare();

    soundPlayer.init();
    renderer.init();
}

namespace
{
psyqo::Coroutine<> loadCoroutine(Game& game)
{
    game.levelId = startLevel;

    psyqo::Coroutine<>::Awaiter awaiter = game.gameLoadCoroutine.awaiter();

    game.loadModel(getLevelModelPath(game.levelId), game.levelModel);
    co_await awaiter;

    game.loadModel(getLevelModelPath(1), game.level2Model);
    co_await awaiter;

    game.loadModel("CATO.BIN;1", game.catoModel);
    co_await awaiter;

    game.loadModel("HUMAN.BIN;1", game.humanModel);
    co_await awaiter;

    game.loadModel("CAR.BIN;1", game.carModel);
    co_await awaiter;

    game.loadTIM("BRICKS.TIM;1", game.bricksTexture);
    co_await awaiter;

    game.loadTIM("CATO.TIM;1", game.catoTexture);
    co_await awaiter;

    game.loadTIM("CAR.TIM;1", game.carTexture);
    co_await awaiter;

    game.loadTIM("FONT.TIM;1", game.fontTexture);
    co_await awaiter;

    game.loadFont("FONT.FNT;1", game.font);
    co_await awaiter;

    game.loadAnimations("HUMAN.ANM;1", game.animations);
    co_await awaiter;

    game.loadMIDI("SONG.MID;1", game.midi);
    co_await awaiter;

    game.loadInstruments("INST.VAB;1", game.vab);
    co_await awaiter;

    game.loadRawPCM("SMPL.PCM;1", 0x1010);
    co_await awaiter;

    game.step1Sound = 0x3300;
    game.loadSound("STEP1.VAG;1", game.step1Sound);
    co_await awaiter;

    game.step2Sound = 0x3F00;
    game.loadSound("STEP2.VAG;1", game.step2Sound);
    co_await awaiter;

    game.popScene(); // pop loading scene
    game.pushScene(&game.gameplayScene);
}
}

void Game::createScene()
{
    romFont.uploadSystemFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});

    pad.init();

    pushScene(&loadingScene);

    // load resources
    gameLoadCoroutine = loadCoroutine(*this);
    gameLoadCoroutine.resume();
}

void Game::loadTIM(eastl::string_view filename, TextureInfo& textureInfo)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, &textureInfo](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            const auto tim = readTimFile(cdReadBuffer);
            textureInfo = uploadTIM(tim);
            gameLoadCoroutine.resume();
        });
}

void Game::loadFont(eastl::string_view filename, Font& font)
{
    cdromLoader
        .readFile(filename, gpu(), isoParser, [this, &font](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            font.loadFromFile(cdReadBuffer);
            gameLoadCoroutine.resume();
        });
}

void Game::loadModel(eastl::string_view filename, Model& model)
{
    cdromLoader
        .readFile(filename, gpu(), isoParser, [this, &model](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            model.load(cdReadBuffer);
            gameLoadCoroutine.resume();
        });
}

void Game::loadSound(eastl::string_view filename, uint32_t spuUploadAddr)
{
    cdromLoader.readFile(
        filename,
        gpu(),
        isoParser,
        [this, filename, spuUploadAddr](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            Sound sound;
            sound.load(filename, cdReadBuffer);
            soundPlayer.uploadSound(spuUploadAddr << 3, sound);
            gameLoadCoroutine.resume();
        });
}

void Game::loadMIDI(eastl::string_view filename, MidiFile& midi)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, filename, &midi](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            midi.load(filename, cdReadBuffer);
            gameLoadCoroutine.resume();
        });
}

void Game::loadInstruments(eastl::string_view filename, VabFile& vab)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, filename, &vab](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            vab.load(filename, cdReadBuffer);
            gameLoadCoroutine.resume();
        });
}

void Game::loadRawPCM(eastl::string_view filename, uint32_t spuUploadAddr)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, spuUploadAddr](eastl::vector<uint8_t>&& buffer) {
            soundPlayer.uploadSound(spuUploadAddr, buffer.data(), buffer.size());
            gameLoadCoroutine.resume();
        });
}

void Game::loadAnimations(eastl::string_view filename, eastl::vector<SkeletalAnimation>& animations)
{
    cdromLoader
        .readFile(filename, gpu(), isoParser, [this, &animations](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            ::loadAnimations(cdReadBuffer, animations);
            gameLoadCoroutine.resume();
        });
}

TextureInfo Game::uploadTIM(const TimFile& tim)
{
    psyqo::Rect region =
        {.pos = {{.x = (std::int16_t)tim.pixDX, .y = (std::int16_t)tim.pixDY}},
         .size = {{.w = (std::int16_t)tim.pixW, .h = (std::int16_t)tim.pixH}}};
    gpu().uploadToVRAM((uint16_t*)tim.pixelsIdx.data(), region);

    // upload CLUT(s)
    // FIXME: support multiple cluts
    region =
        {.pos = {{.x = (std::int16_t)tim.clutDX, .y = (std::int16_t)tim.clutDY}},
         .size = {{.w = (std::int16_t)tim.clutW, .h = (std::int16_t)tim.clutH}}};
    gpu().uploadToVRAM(tim.cluts[0].colors.data(), region);

    TextureInfo info;
    info.clut = {{.x = tim.clutDX, .y = tim.clutDY}};

    const auto colorMode = [](TimFile::PMode pmode) {
        switch (pmode) {
        case TimFile::PMode::Clut4Bit:
            return psyqo::Prim::TPageAttr::Tex4Bits;
            break;
        case TimFile::PMode::Clut8Bit:
            return psyqo::Prim::TPageAttr::Tex8Bits;
            break;
        case TimFile::PMode::Direct15Bit:
            return psyqo::Prim::TPageAttr::Tex16Bits;
            break;
        }
        psyqo::Kernel::assert(false, "unexpected pmode value");
        return psyqo::Prim::TPageAttr::Tex16Bits;
    }(tim.pmode);

    info.tpage.setPageX((std::uint8_t)(tim.pixDX / 64))
        .setPageY((std::uint8_t)(tim.pixDY / 128))
        .setDithering(true)
        .set(colorMode);

    return info;
}
