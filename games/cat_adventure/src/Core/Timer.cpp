#include "Timer.h"

Timer::Timer(std::uint32_t tickTime) : tickTime(tickTime)
{
    reset();
}

void Timer::reset()
{
    time = 0;
    ticked = false;
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
