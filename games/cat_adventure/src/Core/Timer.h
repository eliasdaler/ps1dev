#pragma once

#include <cstdint>

class Timer {
public:
    Timer();
    Timer(uint32_t tickTime);
    void reset();
    void reset(uint32_t newTickTime);
    void update();
    bool tick() const { return ticked; }

private:
    std::uint32_t time;
    bool ticked{false};
    std::uint32_t tickTime;
};
