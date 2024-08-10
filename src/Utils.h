#pragma once

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <sys/types.h>

namespace util
{
eastl::vector<u_long> readFile(eastl::string_view filename);

inline char GetChar(const u_char* bytes, u_long* b)
{
    return bytes[(*b)++];
}

inline short GetShortLE(u_char* bytes, u_long* b)
{
    u_short value = 0;
    value |= bytes[(*b)++] << 0;
    value |= bytes[(*b)++] << 8;
    return (short)value;
}

inline short GetShortBE(const u_char* bytes, u_long* b)
{
    u_short value = 0;
    value |= bytes[(*b)++] << 8;
    value |= bytes[(*b)++] << 0;
    return (short)value;
}

inline long GetLongLE(const u_char* bytes, u_long* b)
{
    u_long value = 0;
    value |= bytes[(*b)++] << 0;
    value |= bytes[(*b)++] << 8;
    value |= bytes[(*b)++] << 16;
    value |= bytes[(*b)++] << 24;
    return (long)value;
}

inline long GetLongBE(const u_char* bytes, u_long* b)
{
    u_long value = 0;
    value |= bytes[(*b)++] << 24;
    value |= bytes[(*b)++] << 16;
    value |= bytes[(*b)++] << 8;
    value |= bytes[(*b)++] << 0;
    return (long)value;
}

};
