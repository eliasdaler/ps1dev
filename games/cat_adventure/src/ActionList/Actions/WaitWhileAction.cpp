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

bool WaitWhileAction::update(std::uint32_t dt)
{
    bool res = f(dt);
    return !res;
}
