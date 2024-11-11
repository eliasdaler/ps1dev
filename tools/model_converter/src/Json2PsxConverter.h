#pragma once

#include "ConversionParams.h"

struct PsxModel;
struct ModelJson;

PsxModel jsonToPsxModel(const ModelJson& modelJson, const ConversionParams& params);
