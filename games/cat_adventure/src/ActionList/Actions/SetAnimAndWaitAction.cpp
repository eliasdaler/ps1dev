#include "SetAnimAndWaitAction.h"

#include <Object.h>

SetAnimAndWaitAction::SetAnimAndWaitAction(AnimatedModelObject& object, StringHash animName) :
    objectPtr(&object), animName(animName)
{}

SetAnimAndWaitAction::SetAnimAndWaitAction(
    AnimatedModelObject& object,
    StringHash animName,
    StringHash faceAnimName) :
    objectPtr(&object), animName(animName), faceAnimName(faceAnimName)
{}

bool SetAnimAndWaitAction::enter()
{
    objectPtr->animator.setAnimation(animName);
    if (faceAnimName) {
        objectPtr->setFaceAnimation(faceAnimName);
    }
    return false;
}

bool SetAnimAndWaitAction::update(std::uint32_t dt)
{
    return objectPtr->animator.animationJustEnded();
}
