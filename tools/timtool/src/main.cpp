#include <cassert>

#include "TimCreateConfig.h"
#include "TimFile.h"
#include "TimFileCreator.h"
#include "TimFileWrite.h"
// #include "TimFileRead.h"

#include <CLI/CLI.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "Font.h"

int main(int argc, char* argv[])
{
    CLI::App cliApp{};

    std::filesystem::path timsJsonPath;
    cliApp.add_option("TIMSJSON", timsJsonPath, "TIM description JSON file")
        ->required()
        ->check(CLI::ExistingFile);

    try {
        cliApp.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(cliApp.exit(e));
    }

    nlohmann::json root;
    std::ifstream file(timsJsonPath);
    assert(file.good());
    file >> root;

    bool hadErrors = false;
    const auto rootDir = std::filesystem::path(timsJsonPath).parent_path();

    // process images
    const auto& imagesRoot = root.at("images");
    for (const auto& cObj : imagesRoot) {
        try {
            const auto config = readConfig(rootDir, cObj, false);
            const auto tim = createTimFile(config);
            std::cout << config.inputImage << " -> " << config.outputFile << std::endl;
            writeTimFile(tim, config.outputFile);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    // process fonts
    if (auto it = root.find("fonts"); it != root.end()) {
        const auto& fontsRoot = *it;
        for (const auto& cObj : fontsRoot) {
            try {
                auto config = readConfig(rootDir, cObj, true);
                std::cout << "FONT:" << config.inputFont << " -> atlas: " << config.outputFile
                          << ", info: " << config.glyphInfoOutputPath << std::endl;

                Font font;
                font.load(config.inputFont, config.size, false);

                ImageData imageData{};
                imageData.pixels = font.atlasBitmapData; // TODO: avoid copy?
                imageData.width = font.atlasSize.x;
                imageData.height = font.atlasSize.y;
                imageData.channels = 3;

                config.pixelData = &imageData;
                const auto tim = createTimFile(config);
                writeTimFile(tim, config.outputFile);
                font.writeFontInfo(config.glyphInfoOutputPath);
            } catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
            }
        }
    }

    if (hadErrors) {
        return 1;
    }
}
