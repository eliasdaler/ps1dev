#pragma once

#include <psyqo/fixed-point.hh>

namespace psyqo
{
struct GPU;
};

struct FPSCounter {
    void update(const psyqo::GPU& gpu);

    const psyqo::FixedPoint<> getMovingAverage() const { return fpsMovingAverageNew; }
    const psyqo::FixedPoint<> getAverage() const { return avgFPS; }

    int frameDiff{0};
    int lastFrameCounter{0};

    // using moving average
    psyqo::FixedPoint<> alpha{0.8};
    psyqo::FixedPoint<> oneMinAlpha{1.0 - 0.8};
    psyqo::FixedPoint<> fpsMovingAverageOld{};
    psyqo::FixedPoint<> fpsMovingAverageNew{};

    // using lerp
    psyqo::FixedPoint<> newFPS{0.f};
    psyqo::FixedPoint<> avgFPS{0.f};
    psyqo::FixedPoint<> lerpFactor{0.1};
};
