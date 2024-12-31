#pragma once

#include <filesystem>
#include <vector>

#include "ImageLoader.h"

#include "../timtool/src/TimFile.h"

struct TextureData {
    int materialIdx{-1};
    std::filesystem::path filename;

    // precomputed
    int tpage;
    int tpagePlusOne;
    int clut;

    TimFile::PMode pmode;
    ImageData imageData;
};

class TexturesData {
public:
    void load(const std::filesystem::path& assetDirPath, const std::filesystem::path& path);

    const TextureData& get(const std::filesystem::path& path) const;

private:
    std::vector<TextureData> textures;
};
