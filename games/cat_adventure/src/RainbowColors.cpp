#include "RainbowColors.h"

namespace
{
const eastl::array<psyqo::Color, 8> RAINBOW_COLORS = {
    psyqo::Color{{251, 242, 54}},
    psyqo::Color{{255, 80, 32}},
    psyqo::Color{{255, 67, 67}},
    psyqo::Color{{50, 255, 80}},
    psyqo::Color{{90, 126, 207}},
    psyqo::Color{{100, 80, 128}},
    psyqo::Color{{255, 67, 170}},
    psyqo::Color{{255, 255, 255}},
};
}

const psyqo::Color& getRainbowColor(std::uint32_t seed)
{
    return RAINBOW_COLORS[seed % 8];
}
