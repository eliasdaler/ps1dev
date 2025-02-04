#include "SayAction.h"

#include <Object.h>
#include <UI/DialogueBox.h>

SayAction::SayAction(DialogueBox& dialogueBox, eastl::string_view text) :
    dialogueBox(dialogueBox), text(text)
{}

SayAction::SayAction(DialogueBox& dialogueBox, eastl::string_view text, const SayParams& params) :
    dialogueBox(dialogueBox), text(text), params(params)
{}

bool SayAction::enter()
{
    dialogueBox.setText(text.data());
    if (params.cameraPtr) {
        params.cameraPtr->setTransform(params.cameraTransform);
    }
    if (params.object) {
        if (params.anim) {
            params.object->animator.setAnimation(params.anim);
        }
        if (params.faceAnim) {
            params.object->setFaceAnimation(params.faceAnim);
        }
    }
    return false;
}

bool SayAction::isFinished() const
{
    return dialogueBox.wantClose;
}
