#include "Game.h"

#include <common/syscalls/syscalls.h>

#include "StringHashes.h"

Game::Game() : renderer(gpu()), songPlayer(gpu(), soundPlayer)
{}

void Game::prepare()
{
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::NTSC)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
    cdrom.prepare();
    soundPlayer.init();

    initStringHashes();
    renderer.init();
}

void Game::loadTIM(eastl::string_view filename, TextureInfo& textureInfo)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, &textureInfo](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            const auto tim = readTimFile(cdReadBuffer);
            textureInfo = uploadTIM(tim);
            cdLoadCoroutine.resume();
        });
}

void Game::loadFont(eastl::string_view filename, Font& font)
{
    cdromLoader
        .readFile(filename, gpu(), isoParser, [this, &font](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            font.loadFromFile(cdReadBuffer);
            cdLoadCoroutine.resume();
        });
}

void Game::loadModel(eastl::string_view filename, Model& model)
{
    cdromLoader
        .readFile(filename, gpu(), isoParser, [this, &model](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            model.load(cdReadBuffer);
            cdLoadCoroutine.resume();
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
            cdLoadCoroutine.resume();
        });
}

void Game::loadMIDI(eastl::string_view filename, MidiFile& midi)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, filename, &midi](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            midi.load(filename, cdReadBuffer);
            cdLoadCoroutine.resume();
        });
}

void Game::loadInstruments(eastl::string_view filename, VabFile& vab)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, filename, &vab](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            vab.load(filename, cdReadBuffer);
            cdLoadCoroutine.resume();
        });
}

void Game::loadRawPCM(eastl::string_view filename, uint32_t spuUploadAddr)
{
    cdromLoader.readFile(
        filename, gpu(), isoParser, [this, spuUploadAddr](eastl::vector<uint8_t>&& buffer) {
            soundPlayer.uploadSound(spuUploadAddr, buffer.data(), buffer.size());
            cdLoadCoroutine.resume();
        });
}

void Game::loadAnimations(eastl::string_view filename, eastl::vector<SkeletalAnimation>& animations)
{
    cdromLoader
        .readFile(filename, gpu(), isoParser, [this, &animations](eastl::vector<uint8_t>&& buffer) {
            cdReadBuffer = eastl::move(buffer);
            ::loadAnimations(cdReadBuffer, animations);
            cdLoadCoroutine.resume();
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

    // ramsyscall_printf("tim pixDX: %d, pixDY: %d\n", tim.pixDX, tim.pixDY);

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
