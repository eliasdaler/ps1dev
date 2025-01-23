#include "DoAction.h"

DoAction::DoAction(eastl::function<void()> f) : f(eastl::move(f))
{}

DoAction::DoAction(eastl::string name, eastl::function<void()> f) :
    name(eastl::move(name)), f(eastl::move(f))
{}

bool DoAction::enter()
{
    f();
    return true;
}
