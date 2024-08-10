#pragma once

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <sys/types.h>

namespace util
{
eastl::vector<u_long> readFile(eastl::string_view filename);
};
