#include <cassert>
#include <filesystem>
#include <iostream>

#include "Json2PsxConverter.h"
#include "ModelJsonFile.h"
#include "PsxModel.h"

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

    if (inputFilePath.extension() == ".json") {
        const auto modelJson = parseJsonFile(inputFilePath, assetDirPath);
        if (fastModel) {
            assert(false && "not implemented");
        } else {
            const auto psxModel = jsonToPsxModel(modelJson, conversionParams);
            writePsxModel(psxModel, outputFilePath);
        }
    } else {
        assert(false && "unsupported format");
    }

    std::cout << "Done!" << std::endl;
}
