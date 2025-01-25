#include "SayAction.h"

#include <UI/DialogueBox.h>

SayAction::SayAction(DialogueBox& dialogueBox, eastl::string_view text) :
    dialogueBox(dialogueBox), text(text)
{}

SayAction::SayAction(
    DialogueBox& dialogueBox,
    eastl::string_view text,
    Camera& camera,
    const CameraTransform& transform) :
    dialogueBox(dialogueBox),
    text(text),
    cameraPtr(&camera),
    cameraTransform(transform),
    hasCameraTransform(true)
{}

bool SayAction::enter()
{
    dialogueBox.setText(text.data());
    if (hasCameraTransform) {
        cameraPtr->setTransform(cameraTransform);
    }
    return false;
}

bool SayAction::update(std::uint32_t dt)
{
    return dialogueBox.wantClose;
}
