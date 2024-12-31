#include <cassert>
#include <filesystem>
#include <iostream>

#include "AnimationWriter.h"
#include "Json2PsxConverter.h"
#include "LevelJsonFile.h"
#include "LevelWriter.h"
#include "ModelJsonFile.h"
#include "PsxModel.h"
#include "TexturesData.h"

#include <CLI/CLI.hpp>

int main(int argc, char* argv[])
{
    CLI::App cliApp{};

    bool fastModel{false};
    cliApp.add_flag("--fast-model", fastModel, "Make fast model");

    std::filesystem::path inputFilePath;
    cliApp.add_option("INPUTFILE", inputFilePath, "Input file")
        ->required()
        ->check(CLI::ExistingFile);

    std::filesystem::path outputFilePath;
    cliApp.add_option("OUTPUTFILE", outputFilePath, "Output file")->required();

    std::filesystem::path assetDirPath;
    cliApp.add_option("ASSETDIR", assetDirPath, "Asset dir")
        ->required()
        ->check(CLI::ExistingDirectory);

    try {
        cliApp.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(cliApp.exit(e));
    }

    cliApp.validate_positionals();

    std::cout << "in: " << inputFilePath << std::endl;
    std::cout << "out: " << outputFilePath << std::endl;
    std::cout << "asset dir: " << assetDirPath << std::endl;

    // convert
    ConversionParams conversionParams{
        .scale = 1.f / 8.f,
    };

    const auto modelJson = parseJsonFile(inputFilePath, assetDirPath);

    TexturesData textures;
    textures.load(assetDirPath, assetDirPath / "../tims.json"); // TODO: don't hardcode path

    const auto psxModel = jsonToPsxModel(modelJson, textures, conversionParams);
    if (!modelJson.animations.empty()) {
        auto outAnimPath = outputFilePath;
        outAnimPath.replace_extension(".anm");
        std::cout << "Writing animations to " << outAnimPath << std::endl;
        writeAnimationsToFile(outAnimPath, modelJson.animations, conversionParams);
    }

    bool isLevel = !modelJson.collision.empty(); // TODO: special flag?

    if (isLevel) {
        auto outLevelPath = outputFilePath;
        outLevelPath.replace_extension(".lvl");

        auto levelMetaPath =
            assetDirPath / "levels" / inputFilePath.stem().replace_extension(".json");
        std::cout << "level metadata:" << levelMetaPath << std::endl;
        LevelJson levelJson = parseLevelJsonFile(levelMetaPath, assetDirPath);

        std::cout << "Writing level data to " << outLevelPath << std::endl;
        writeLevelToFile(outLevelPath, modelJson, levelJson, conversionParams);
    }

    if (fastModel) {
        writeFastPsxModel(psxModel, outputFilePath);
    } else {
        writePsxModel(psxModel, outputFilePath);
    }

    std::cout << "Done!" << std::endl;
}
