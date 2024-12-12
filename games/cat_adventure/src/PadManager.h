#pragma once

#include <cstdint>

#include <psyqo/simplepad.hh>

class PadManager {
public:
    void init();
    void update();

    bool wasButtonJustPressed(psyqo::SimplePad::Button button) const;
    bool wasButtonJustReleased(psyqo::SimplePad::Button button) const;
    bool isButtonHeld(psyqo::SimplePad::Button button) const;
    bool isButtonPressed(psyqo::SimplePad::Button button) const;

private:
    psyqo::SimplePad pad;
    std::uint16_t currentState{0};
    std::uint16_t prevState{0}; // state from the previous frame
};
