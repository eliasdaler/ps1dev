#include "DelayAction.h"

#include <common/syscalls/syscalls.h>

DelayAction::DelayAction(std::uint32_t delayDurationSeconds) :
    delayDurationMicroseconds(delayDurationSeconds * 1'000'000)
{}

bool DelayAction::enter()
{
    currentTime = 0;
    return isFinished();
}

void DelayAction::update(std::uint32_t dt)
{
    currentTime += dt;
}

bool DelayAction::isFinished() const
{
    return currentTime >= delayDurationMicroseconds;
}
