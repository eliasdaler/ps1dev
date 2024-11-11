#pragma once

#include <cstdint>

class Timer {
public:
    Timer(uint32_t tickTime);
    void reset();
    void update();
    bool tick() const { return ticked; }

private:
    std::uint32_t time;
    bool ticked{false};
    std::uint32_t tickTime;
};
