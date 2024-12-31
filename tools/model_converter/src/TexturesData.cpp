#include "TexturesData.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../timtool/src/TimCreateConfig.h"

#include "GTETypes.h"

void TexturesData::load(const std::filesystem::path& path)
{
    std::cout << "Reading texture data from " << path << std::endl;

    nlohmann::json root;
    std::ifstream file(path);
    assert(file.good());
    file >> root;

    bool hadErrors = false;
    const auto rootDir = std::filesystem::path(path).parent_path();

    // process images
    const auto& imagesRoot = root.at("images");
    textures.reserve(imagesRoot.size());
    for (const auto& cObj : imagesRoot) {
        try {
            const auto config = readTimConfig(rootDir, cObj, false);

            int tp = [](TimFile::PMode pmode) {
                switch (pmode) {
                case TimFile::PMode::Clut4Bit:
                    return 0;
                case TimFile::PMode::Clut8Bit:
                    return 1;
                default:
                    return 2;
                }
            }(config.pmode);

            textures.push_back(TextureData{
                .materialIdx = (int)textures.size(),
                .filename = config.inputImage.stem(),
                // TODO: set proper transparency mode based on the config
                .tpage = getTPage(tp, 1, config.pixDX, config.pixDY),
                .tpagePlusOne = getTPage(tp, 1, config.pixDX + 64, config.pixDY),
                .clut = getClut(config.clutDX, config.clutDY),
            });
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }
}

const TextureData& TexturesData::get(const std::filesystem::path& path) const
{
    const auto filename = path.stem();
    for (const auto& td : textures) {
        if (td.filename == filename) {
            return td;
        }
    }
    throw std::runtime_error("can't find texture info forr: " + path.string());
}
