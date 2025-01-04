#include "CDLoader.h"

#include <Audio/SoundPlayer.h>
#include <Game.h>
#include <Graphics/TimFile.h>
#include <Level.h>

CDLoader::CDLoader(Game& game) : game(game)
{}

void CDLoader::init()
{
    cdrom.prepare();
}

void CDLoader::loadTIM(eastl::string_view filename, TextureInfo& textureInfo)
{
    loadFromCD(filename, [this, &textureInfo](eastl::vector<uint8_t>&& buffer) {
        const auto tim = readTimFile(buffer);
        textureInfo = game.renderer.uploadTIM(tim);
    });
}

void CDLoader::loadFont(eastl::string_view filename, Font& font)
{
    loadFromCD(filename, [&font](eastl::vector<uint8_t>&& buffer) { font.loadFromFile(buffer); });
}

void CDLoader::loadModel(eastl::string_view filename, ModelData& model)
{
    loadFromCD(filename, [&model](eastl::vector<uint8_t>&& buffer) { model.load(buffer); });
}

void CDLoader::loadSound(eastl::string_view filename, uint32_t spuUploadAddr)
{
    loadFromCD(filename, [this, filename, spuUploadAddr](eastl::vector<uint8_t>&& buffer) {
        Sound sound;
        sound.load(filename, buffer);
        game.soundPlayer.uploadSound(spuUploadAddr << 3, sound);
    });
}

void CDLoader::loadMIDI(eastl::string_view filename, MidiFile& midi)
{
    loadFromCD(filename, [filename, &midi](eastl::vector<uint8_t>&& buffer) {
        midi.load(filename, buffer);
    });
}

void CDLoader::loadInstruments(eastl::string_view filename, VabFile& vab)
{
    loadFromCD(filename, [filename, &vab](eastl::vector<uint8_t>&& buffer) {
        vab.load(filename, buffer);
    });
}

void CDLoader::loadRawPCM(eastl::string_view filename, uint32_t spuUploadAddr)
{
    loadFromCD(filename, [this, spuUploadAddr](eastl::vector<uint8_t>&& buffer) {
        game.soundPlayer.uploadSound(spuUploadAddr, buffer.data(), buffer.size());
    });
}

void CDLoader::loadAnimations(
    eastl::string_view filename,
    eastl::vector<SkeletalAnimation>& animations)
{
    loadFromCD(filename, [&animations](eastl::vector<uint8_t>&& buffer) {
        ::loadAnimations(buffer, animations);
    });
}

void CDLoader::loadLevel(eastl::string_view filename, Level& level)
{
    loadFromCD(filename, [&level](eastl::vector<uint8_t>&& buffer) { level.load(buffer); });
}

void CDLoader::loadFromCD(
    eastl::string_view filename,
    eastl::function<void(eastl::vector<uint8_t>&&)>&& callback)
{
    cdromLoader.readFile(
        filename,
        game.gpu(),
        isoParser,
        [this, cb = eastl::move(callback)](eastl::vector<uint8_t>&& buffer) {
            cb(eastl::move(buffer));
            game.gameLoadCoroutine.resume();
        });
}
