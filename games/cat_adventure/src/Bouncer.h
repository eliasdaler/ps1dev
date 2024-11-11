#pragma once

#include "Timer.h"

class Bouncer {
public:
    Bouncer(std::uint32_t bounceTime, int maxBounceOffset);
    void update();

    int getOffset() const { return offset; }

private:
    Timer timer;
    int offset;
    int maxBounceOffset;
    bool moveDown{true};
};
