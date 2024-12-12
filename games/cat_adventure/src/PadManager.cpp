#include "PadManager.h"

void PadManager::init()
{
    pad.initialize();
}

void PadManager::update()
{
    prevState = currentState;
    currentState = 0;
    for (int i = 0; i < 16; ++i) {
        currentState |=
            (pad.isButtonPressed(psyqo::SimplePad::Pad1, (psyqo::SimplePad::Button)i) ? 1 : 0) << i;
    }
}

bool PadManager::wasButtonJustPressed(psyqo::SimplePad::Button button) const
{
    const auto buttonMask = (1 << (int)button);
    return !(prevState & buttonMask) && (currentState & buttonMask);
}

bool PadManager::wasButtonJustReleased(psyqo::SimplePad::Button button) const
{
    const auto buttonMask = (1 << (int)button);
    return (prevState & buttonMask) && !(currentState & buttonMask);
}

bool PadManager::isButtonHeld(psyqo::SimplePad::Button button) const
{
    const auto buttonMask = (1 << (int)button);
    return (prevState & buttonMask) && (currentState & buttonMask);
}

bool PadManager::isButtonPressed(psyqo::SimplePad::Button button) const
{
    const auto buttonMask = (1 << (int)button);
    return (currentState & buttonMask);
}
