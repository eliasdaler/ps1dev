#pragma once

#include "ConversionParams.h"

struct PsxModel;
struct ModelJson;
struct TexturesData;

PsxModel jsonToPsxModel(
    const ModelJson& modelJson,
    const TexturesData& textures,
    const ConversionParams& params);
