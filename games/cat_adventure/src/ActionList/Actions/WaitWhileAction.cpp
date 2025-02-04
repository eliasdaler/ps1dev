#include "WaitWhileAction.h"

WaitWhileAction::WaitWhileAction(ConditionFuncType f) : f(eastl::move(f))
{}

WaitWhileAction::WaitWhileAction(eastl::string conditionName, ConditionFuncType f) :
    f(eastl::move(f)), conditionName(eastl::move(conditionName))
{}

bool WaitWhileAction::enter()
{
    bool res = f(0);
    return !res;
}

void WaitWhileAction::update(std::uint32_t dt)
{
    finished = (f(dt) == false);
}

bool WaitWhileAction::isFinished() const
{
    return finished;
}
