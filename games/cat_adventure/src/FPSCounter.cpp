#include "FPSCounter.h"

#include <psyqo/gpu.hh>

void FPSCounter::update(const psyqo::GPU& gpu)
{
    const auto currentFrameCounter = gpu.getFrameCount();
    frameDiff = currentFrameCounter - lastFrameCounter;
    lastFrameCounter = currentFrameCounter;

    const auto fps = gpu.getRefreshRate() / frameDiff;
    fpsMovingAverageNew = alpha * fps + oneMinAlpha * fpsMovingAverageOld;
    fpsMovingAverageOld = fpsMovingAverageNew;

    newFPS.value = fps << 12;
    // lerp
    avgFPS = avgFPS + lerpFactor * (newFPS - avgFPS);
}
