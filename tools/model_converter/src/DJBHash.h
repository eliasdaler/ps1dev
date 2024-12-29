#pragma once

#include <cstdint>
#include <string>

namespace
{
struct DJBHash {
public:
    static std::uint32_t hash(const std::string& str)
    {
        return static_cast<std::uint32_t>(djbProcess(seed, str.c_str(), str.size()));
    }

    static std::uint64_t djbProcess(std::uint64_t hash, const char str[], std::size_t n)
    {
        std::uint64_t res = hash;
        for (std::size_t i = 0; i < n; ++i) {
            res = res * 33 + str[i];
        }
        return res;
    }

    static constexpr std::uint64_t seed{5381};
};
}
