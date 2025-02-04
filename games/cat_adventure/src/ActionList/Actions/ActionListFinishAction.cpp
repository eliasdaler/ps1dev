#include "ActionListFinishAction.h"

ActionListFinishAction::ActionListFinishAction(ActionList actionList) :
    actionList(eastl::move(actionList))
{}

bool ActionListFinishAction::enter()
{
    actionList.play();
    return actionList.isFinished();
}

void ActionListFinishAction::update(std::uint32_t dt)
{
    actionList.update(dt);
}

bool ActionListFinishAction::isFinished() const
{
    return actionList.isFinished();
}
