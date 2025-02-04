#pragma once

#include <cstdint>

class Action {
public:
    virtual ~Action(){};

    // return true from "enter" to signify that action is finished
    virtual bool enter() { return true; }

    virtual void update(std::uint32_t dt) {}

    virtual bool isFinished() const = 0;

    bool parallel{false};
};
