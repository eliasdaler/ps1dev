#pragma once

#include <ostream>

namespace fsutil
{
template<typename T>
void binaryWrite(std::ostream& os, const T& val)
{
    os.write(reinterpret_cast<const char*>(&val), sizeof(T));
}
}
