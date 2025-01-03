#include "Timer.h"

Timer::Timer() : tickTime(0)
{
    reset();
}

Timer::Timer(std::uint32_t tickTime) : tickTime(tickTime)
{
    reset();
}

void Timer::reset()
{
    time = 0;
    ticked = false;
}

void Timer::reset(uint32_t newTickTime)
{
    tickTime = newTickTime;
    reset();
}

void Timer::update()
{
    ticked = false;
    ++time;
    if (time == tickTime) {
        time = 0;
        ticked = true;
    }
}
