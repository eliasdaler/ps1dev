#pragma once

#include <cstring>
#include <filesystem>
#include <vector>

#include "Color.h"

struct ImageData {
    ImageData() = default;
    ~ImageData();

    // move only
    ImageData(ImageData&& o) = default;
    ImageData& operator=(ImageData&& o) = default;

    // no copies
    ImageData(const ImageData& o) = delete;
    ImageData& operator=(const ImageData& o) = delete;

    Color32 getPixel(int x, int y) const
    {
        Color32 c{};
        std::memcpy(&c, &pixelsRaw[y * width * channels + x], sizeof(Color32));
        return c;
    }

    // data
    unsigned char* pixelsRaw{nullptr};

    std::vector<Color32> pixels;
    int width{0};
    int height{0};
    int channels{0};

    // HDR only
    float* hdrPixels{nullptr};
    bool hdr{false};
    int comp{0};

    bool shouldSTBFree{false};
};

namespace util
{
ImageData loadImage(const std::filesystem::path& p);
}
