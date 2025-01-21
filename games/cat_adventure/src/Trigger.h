#pragma once

#include <cstdint>

#include "Collision.h"
#include "StringHashes.h"

struct Trigger {
    StringHash name;
    AABB aabb;

    bool wasJustEntered() const { return !wasEntered && isEntered; }
    bool wasJustExited() const { return wasEntered && !isEntered; }

    bool interaction{false};
    bool wasEntered{false};
    bool isEntered{false};
};
