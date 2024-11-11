#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

ImageData::~ImageData()
{
    // FIXME: this is a bad hack needed because move operator doesn't signify
    // if the current ImageData still holds pixels or not
    if (pixels.empty()) {
        return;
    }

    if (shouldSTBFree) {
        stbi_image_free(pixelsRaw);
        stbi_image_free(hdrPixels);
    }
}

namespace util
{
ImageData loadImage(const std::filesystem::path& p)
{
    ImageData data;
    data.shouldSTBFree = true;
    if (stbi_is_hdr(p.string().c_str())) {
        data.hdr = true;
        data.hdrPixels = stbi_loadf(p.string().c_str(), &data.width, &data.height, &data.comp, 4);
    } else {
        data.pixelsRaw =
            stbi_load(p.string().c_str(), &data.width, &data.height, &data.channels, 4);

        // TODO: maybe can do this more efficiently by copying the whole thing?
        data.pixels.resize(data.width * data.height);
        for (std::size_t i = 0; i < data.width * data.height; ++i) {
            std::memcpy(&data.pixels[i], &data.pixelsRaw[i * 4], sizeof(Color32));
        }
    }
    data.channels = 4;
    return data;
}

} // namespace util
