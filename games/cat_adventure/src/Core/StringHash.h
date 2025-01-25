#pragma once

#include <compare>
#include <cstdint>

#include <EASTL/map.h>
#include <EASTL/string.h>
//  #include <EASTL/unordered_map.h>

#define DEBUG_HASH

struct StringHash;

struct StringHashMap {
    static const eastl::string& getStr(StringHash hash);
    static const char* getCStr(StringHash hash);
    static void putString(StringHash hash);
    static void putString(StringHash hash, const char* str);

    static eastl::map<std::uint32_t, eastl::string> map;
};

#ifdef DEBUG_HASH
#define HASH_PUT(str) StringHashMap::putString(str##_sh, str)
#define HASH_PUT2(h) StringHashMap::putString(h)
#define FROM_HASH(hash) (StringHashMap::getCStr(hash))
#else
#define HASH_PUT(str)
#define HASH_PUT2(h)
#define FROM_HASH(hash) "???"
#endif

struct StringHash {
    std::uint32_t value{0};
    const char* str{nullptr}; // set when created using _sh string literal

    const char* getStr() const
    {
        if (str) {
            return str;
        }
        return FROM_HASH(*this);
    }

    operator bool() const { return value != 0; }

    bool operator==(const StringHash& o) const { return value == o.value; }
    bool operator<(const StringHash& o) const { return value < o.value; }
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

    /* TODO: don't copy-paste? */
    static inline std::uint64_t djbProcessRuntime(
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
    return StringHash{
        .value = (std::uint32_t)DJBHash::djbProcess(DJBHash::seed, str, n),
        .str = str,
    };
}

struct StringViewWithHash {
    eastl::string_view str;
    StringHash hash;

    StringViewWithHash(eastl::string_view str) :
        str(str), hash(DJBHash::djbProcessRuntime(DJBHash::seed, str.begin(), str.size()))
    {}
};
