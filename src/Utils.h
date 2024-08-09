#pragma once

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

namespace util
{
eastl::vector<char> readFile(eastl::string_view filename);
};
