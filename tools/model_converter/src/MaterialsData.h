#pragma once

#include <filesystem>
#include <vector>

class MaterialsData {
public:
    void loadMaterialsData(const std::filesystem::path& timsJson);

    struct MaterialData {
        int materialIdx{-1};
        std::filesystem::path filename;

        // precomputed
        int tpage;
        int clut;
    };

    const MaterialData& getMaterialDataByFilename(const std::filesystem::path& path) const;

private:
    std::vector<MaterialData> materials;
};
