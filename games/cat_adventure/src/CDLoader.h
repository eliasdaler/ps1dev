#pragma once

#include <psyqo-paths/cdrom-loader.hh>
#include <psyqo/cdrom-device.hh>

#include <EASTL/vector.h>

struct TextureInfo;
struct Font;
struct Model;
struct MidiFile;
struct VabFile;
struct SkeletalAnimation;
struct Level;

class Game;

struct CDLoader {
    CDLoader(Game& game);
    void init();

    void loadTIM(eastl::string_view filename, TextureInfo& textureInfo);
    void loadFont(eastl::string_view filename, Font& font);
    void loadFastModel(eastl::string_view filename, Model& model);
    void loadSound(eastl::string_view filename, uint32_t spuUploadAddr);
    void loadMIDI(eastl::string_view filename, MidiFile& midi);
    void loadInstruments(eastl::string_view filename, VabFile& vab);
    void loadRawPCM(eastl::string_view filename, uint32_t spuUploadAddr);
    void loadAnimations(eastl::string_view filename, eastl::vector<SkeletalAnimation>& animations);
    void loadLevel(eastl::string_view filename, Level& level);

    void loadFromCD(
        eastl::string_view filename,
        eastl::function<void(eastl::vector<uint8_t>&&)>&& callback);

    psyqo::CDRomDevice cdrom;
    psyqo::ISO9660Parser isoParser{&cdrom};
    psyqo::paths::CDRomLoader cdromLoader;

    Game& game;
};
