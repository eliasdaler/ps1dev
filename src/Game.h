#pragma once

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include <cstdint>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/span.h>
#include <EASTL/unique_ptr.h>

#include "FastModel.h"
#include "Model.h"
#include "Object.h"
#include "Renderer.h"

struct TexRegion {
    int u0, v0; // top left
    int u3, v3; // bottom right
};

struct Quad {
    SVECTOR vs[4];
};

class Game {
public:
    void init();
    void run();
    void handleInput();
    void update();
    void draw();

private:
    std::uint16_t addTexture(TIM_IMAGE texture);

    FastModel rollModel;
    Model levelModel;

    static constexpr int numRolls{8};
    FastModelObject rolls[numRolls];

    ModelObject level;

    eastl::vector<TIM_IMAGE> textures;
    std::uint16_t bricksTextureIdx;
    std::uint16_t rollTextureIdx;

    Renderer renderer;
};
