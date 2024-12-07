#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>

template<typename T>
T floatToFixed(float v, float scaleFactor = 1.f)
{
    constexpr auto scale = 1 << 12;
    float ld = v * scaleFactor;
    static constexpr auto two_to_19 = 524288;
    if (sizeof(T) == 2 && std::abs(ld) > 8 || sizeof(T) == 4 && std::abs(ld) > two_to_19) {
        static const auto fpTypeName = sizeof(T) == 2 ? "4.12" : "20.12";
        throw std::runtime_error(
            std::string("some vertex position is out of ") + fpTypeName + std::string("range:") +
            std::to_string(ld));
    }
    bool negative = ld < 0;
    T integer = negative ? -ld : ld;
    T fraction = ld * scale - integer * scale + (negative ? -0.5 : 0.5);
    return integer * scale + fraction;
}
