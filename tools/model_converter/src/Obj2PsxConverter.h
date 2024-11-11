#pragma once

#include "ConversionParams.h"

struct PsxModel;
struct ObjModel;

PsxModel objToPsxModel(const ObjModel& objModel, const ConversionParams& params);
