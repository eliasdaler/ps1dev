#pragma once

#include <filesystem>
#include <vector>

struct TextureData {
    int materialIdx{-1};
    std::filesystem::path filename;

    // precomputed
    int tpage;
    int clut;
};

class TexturesData {
public:
    void load(const std::filesystem::path& path);

    const TextureData& get(const std::filesystem::path& path) const;

private:
    std::vector<TextureData> textures;
};
