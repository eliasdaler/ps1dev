#include "StringHash.h"

#include <common/syscalls/syscalls.h>

eastl::map<std::uint32_t, eastl::string> StringHashMap::map;

const eastl::string& StringHashMap::getStr(StringHash hash)
{
    const auto it = map.find(hash.value);
    if (it != map.end()) {
        return it->second;
    }
    static const eastl::string str = "???";
    return str;
};

const char* StringHashMap::getCStr(StringHash hash)
{
    const auto it = map.find(hash.value);
    if (it != map.end()) {
        return it->second.c_str();
    }
    ramsyscall_printf(
        "[!!!] StringHashMap::gestCStr: string wasn't found for hash %u\n", hash.value);
    return "???";
};

void StringHashMap::putString(StringHash hash)
{
    putString(hash, hash.str);
}

void StringHashMap::putString(StringHash hash, const char* str)
{
    map.emplace(hash.value, str);
}
