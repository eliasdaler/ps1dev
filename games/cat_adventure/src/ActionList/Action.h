#pragma once

#include <cstdint>

class Action {
public:
    virtual ~Action(){};

    // return true from "enter" to signify that action is finished
    virtual bool enter() { return true; }

    // return true from "update" to signify that action is finished
    // delta time is in microseconds
    virtual bool update(std::uint32_t dt) { return true; }
};
