#pragma once

#include <cstdint>

#include <psyqo/advancedpad.hh>
#include <psyqo/simplepad.hh>

class PadManager {
public:
    void init();
    void update();

    bool wasButtonJustPressed(psyqo::SimplePad::Button button) const;
    bool wasButtonJustReleased(psyqo::SimplePad::Button button) const;
    bool isButtonHeld(psyqo::SimplePad::Button button) const;
    bool isButtonPressed(psyqo::SimplePad::Button button) const;

    int getLeftAxisX();
    int getLeftAxisY();

    int getPadType() { return pad.getPadType(psyqo::AdvancedPad::Pad::Pad1a); }

private:
    // psyqo::SimplePad pad;
    psyqo::AdvancedPad pad;
    std::uint16_t currentState{0};
    std::uint16_t prevState{0}; // state from the previous frame
};
