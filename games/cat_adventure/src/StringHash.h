#pragma once

#include <compare>
#include <cstdint>

#include <EASTL/string.h>
#include <EASTL/unordered_map.h>

#define DEBUG_HASH

struct StringHash;

struct StringHashMap {
    static const eastl::string& getStr(StringHash hash);
    static const char* getCStr(StringHash hash);
    static void putString(StringHash hash, const char* str);

    static eastl::unordered_map<std::uint32_t, eastl::string> map;
};

#ifdef DEBUG_HASH
#define HASH_PUT(str) StringHashMap::putString(str##_sh, str)
#define FROM_HASH(hash) (StringHashMap::getCStr(hash))
#else
#define HASH_PUT(str)
#define FROM_HASH(hash) "???"
#endif

struct StringHash {
    std::uint32_t value;

    const char* getStr() const { return FROM_HASH(*this); }

    auto operator<=>(const StringHash&) const = default;
};

struct DJBHash {
public:
    template<std::size_t N>
    static inline consteval std::uint64_t ctHash(const char (&str)[N])
    {
        return djbProcess(seed, str, N - 1);
    }

    template<std::size_t N>
    static inline consteval std::uint32_t ctHash32(const char (&str)[N])
    {
        return static_cast<std::uint32_t>(djbProcess(seed, str, N - 1));
    }

    static inline consteval std::uint64_t djbProcess(
        std::uint64_t hash,
        const char str[],
        std::size_t n)
    {
        std::uint64_t res = hash;
        for (std::size_t i = 0; i < n; ++i) {
            res = res * 33 + str[i];
        }
        return res;
    }

    static constexpr std::uint64_t seed{5381};
};

inline StringHash consteval operator""_sh(const char* str, std::size_t n)
{
    return StringHash{(std::uint32_t)DJBHash::djbProcess(DJBHash::seed, str, n)};
}
