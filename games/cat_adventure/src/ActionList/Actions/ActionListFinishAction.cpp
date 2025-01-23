#include "ActionListFinishAction.h"

ActionListFinishAction::ActionListFinishAction(ActionList actionList) :
    actionList(eastl::move(actionList))
{}

bool ActionListFinishAction::enter()
{
    actionList.play();
    return actionList.isFinished();
}

bool ActionListFinishAction::update(std::uint32_t dt)
{
    actionList.update(dt);
    return actionList.isFinished();
}
