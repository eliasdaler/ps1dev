#include "PadManager.h"

#include <common/syscalls/syscalls.h>

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
            (pad.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, (psyqo::AdvancedPad::Button)i) ?
                    1 :
                    0)
            << i;
    }
}

int PadManager::getLeftAxisX()
{
    return pad.getAdc(psyqo::AdvancedPad::Pad::Pad1a, 2);
}

int PadManager::getLeftAxisY()
{
    return pad.getAdc(psyqo::AdvancedPad::Pad::Pad1a, 3);
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
