#include "SayAction.h"

#include <UI/DialogueBox.h>

SayAction::SayAction(DialogueBox& dialogueBox, eastl::string_view text) :
    dialogueBox(dialogueBox), text(text)
{}

bool SayAction::enter()
{
    dialogueBox.setText(text.data());
    return false;
}

bool SayAction::update(std::uint32_t dt)
{
    return dialogueBox.wantClose;
}
