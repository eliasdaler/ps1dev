#include "LerpAction.h"

LerpAction::LerpAction(LerpFuncType f, psyqo::FixedPoint<> lerpTime) :
    f(eastl::move(f)), lerpDurationMicroseconds((lerpTime * 1000).integer() * 1000)
{}

bool LerpAction::enter()
{
    currentTime = 0;
    f(0.0);
    return false;
}

void LerpAction::update(std::uint32_t dt)
{
    currentTime += dt;

    auto lerpF = psyqo::FixedPoint<>(currentTime / 1000, 0) /
                 psyqo::FixedPoint<>(lerpDurationMicroseconds / 1000, 0);
    if (lerpF > 1.0) {
        lerpF = 1.0;
    }

    f(lerpF);
}

bool LerpAction::isFinished() const
{
    return currentTime >= lerpDurationMicroseconds;
}
