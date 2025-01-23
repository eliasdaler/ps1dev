#pragma once

#include <EASTL/unique_ptr.h>

#include <ActionList/Action.h>

namespace actions
{
eastl::unique_ptr<Action> delay(std::uint32_t delayDurationSeconds);

}
