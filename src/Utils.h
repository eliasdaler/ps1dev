#pragma once

#include <cstdint>
#include <string.h>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/span.h>

namespace util
{
eastl::vector<uint8_t> readFile(eastl::string_view filename);

struct FileReader {
    const std::uint8_t* bytes;
    int cursor{0};

    std::uint8_t GetUInt8() { return static_cast<uint8_t>(bytes[cursor++]); }
    std::int8_t GetInt8() { return static_cast<int8_t>(bytes[cursor++]); }
    std::int16_t GetInt16() { return GetObj<std::int16_t>(); }
    std::uint16_t GetUInt16() { return GetObj<std::uint16_t>(); }
    std::int32_t GetInt32() { return GetObj<std::int32_t>(); }

    template<typename T>
    T GetObj()
    {
        T obj{};
        memcpy((void*)&obj, &bytes[cursor], sizeof(T));
        cursor += sizeof(T);
        return obj;
    }

    template<typename T>
    void ReadObj(T& obj)
    {
        memcpy((void*)&obj, &bytes[cursor], sizeof(T));
        cursor += sizeof(T);
    }

    template<typename T>
    void ReadArr(T* arr, std::size_t numElements)
    {
        GetBytes(arr, numElements * sizeof(T));
    }

    void GetBytes(void* dest, std::size_t size)
    {
        memcpy((void*)dest, &bytes[cursor], size);
        cursor += size;
    }
};

};
