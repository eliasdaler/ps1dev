#pragma once

#include <filesystem>

#include <nlohmann/json_fwd.hpp>

#include "Color.h"

#include "ImageLoader.h"

#include "TimFile.h"

struct TimCreateConfig {
    std::filesystem::path inputImage;
    std::filesystem::path outputFile;

    // if not empty, read from pixelData directly
    ImageData* pixelData{nullptr};

    // TODO: field for specifying 1-bit images

    std::uint16_t clutDX;
    std::uint16_t clutDY;

    std::uint16_t pixDX;
    std::uint16_t pixDY;

    TimFile::PMode pmode{TimFile::PMode::Clut8Bit};

    // If true, sets STP on all non-black colors of the image
    bool setSTPOnNonBlack{false};

    // If true, sets STP on pure-black pixels
    // This results in semi-transparent black when translucency is on
    // (instead of black pixels becoming fully-transparent)
    bool setSTPOnBlack{false};

    // If true, converts black pixels to (1, 0, 0, 1)
    // (or (1, 0, 0, 0) if setSTPOnBlack is false)
    // which makes black pixels semi-transparent when translucency is on
    // (otherwise, if nonTransparentBlack == false and setSTPOnBlack == false, it
    // will result in (0, 0, 0, 0) pixels - fully transparent pixels
    bool nonTransparentBlack{true};

    // Color to use for transparency when useAlphaChannel is false
    Color32 transparencyColor{.r = 0, .g = 0, .b = 0, .a = 255};

    // json = "useAlphaChannel"
    // If true, use alpha channel of the image for setting STP
    // (See alphaThreshold and stpThreshold )
    bool useAlphaChannel{true};

    // If pixel color is <= alphaThreshold, it's considered fully transparent:
    // (color gets converted to (0, 0, 0, 0))
    std::uint8_t alphaThreshold{0};

    // If pixel color is < alphaThreshold, it's considered semi-transparent:
    // (color gets converted to (r, g, b, 1))
    std::uint8_t stpThreshold{255};

    // Font only
    int size{0}; // font size
    std::filesystem::path inputFont; // location of the font
    std::filesystem::path glyphInfoOutputPath; // where to write glyph info
};

TimCreateConfig readTimConfig(
    const std::filesystem::path& rootDir,
    const nlohmann::json& j,
    bool isFont);
