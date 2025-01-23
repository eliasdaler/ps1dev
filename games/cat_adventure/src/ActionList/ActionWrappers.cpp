#include <ActionList/ActionWrappers.h>

#include <ActionList/Actions/DelayAction.h>

namespace actions
{

eastl::unique_ptr<Action> delay(std::uint32_t delayDurationSeconds)
{
    return eastl::make_unique<DelayAction>(delayDurationSeconds);
}
}
