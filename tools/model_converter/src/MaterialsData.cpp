#include "MaterialsData.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../timtool/src/TimCreateConfig.h"

#include "GTETypes.h"

void MaterialsData::loadMaterialsData(const std::filesystem::path& path)
{
    nlohmann::json root;
    std::ifstream file(path);
    assert(file.good());
    file >> root;

    bool hadErrors = false;
    const auto rootDir = std::filesystem::path(path).parent_path();

    // process images
    const auto& imagesRoot = root.at("images");
    materials.reserve(imagesRoot.size());
    for (const auto& cObj : imagesRoot) {
        try {
            const auto config = readTimConfig(rootDir, cObj, false);

            materials.push_back(MaterialData{
                .materialIdx = (int)materials.size(),
                .filename = config.inputImage.stem(),
                // TODO: store
                .tpage = getTPage(1, 1, config.pixDX, config.pixDY),
                .clut = getClut(config.clutDX, config.clutDY),
            });

        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }
}
