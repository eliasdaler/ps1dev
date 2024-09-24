#pragma once

#include <libgpu.h>

#include "FastModel.h"

struct Model;

struct Object {
    SVECTOR rotation{};
    VECTOR position{};
    VECTOR scale{ONE, ONE, ONE};
};

struct FastModelObject : Object {
    FastModelInstance model;
};

struct ModelObject : Object {
    Model* model{nullptr};
};
