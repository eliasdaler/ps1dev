#pragma once

#include <cstdint>

#include "Collision.h"

struct Trigger {
    AABB aabb;
    std::uint16_t id;

    bool wasJustEntered() const { return !wasEntered && isEntered; }
    bool wasJustExited() const { return wasEntered && !isEntered; }

    bool wasEntered{false};
    bool isEntered{false};
};
