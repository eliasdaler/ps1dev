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

    std::int16_t GetInt16()
    {
        uint16_t value = 0;
        value |= bytes[cursor++];
        value |= bytes[cursor++] << 8;
        return static_cast<int16_t>(value);
    }

    std::uint16_t GetUInt16()
    {
        uint16_t value = 0;
        value |= bytes[cursor++];
        value |= bytes[cursor++] << 8;
        return static_cast<uint16_t>(value);
    }

    std::int16_t GetInt16BE()
    {
        uint16_t value = 0;
        value |= bytes[cursor++] << 8;
        value |= bytes[cursor++];
        return static_cast<int16_t>(value);
    }

    std::int32_t GetInt32()
    {
        uint32_t value = 0;
        value |= bytes[cursor++];
        value |= bytes[cursor++] << 8;
        value |= bytes[cursor++] << 16;
        value |= bytes[cursor++] << 24;
        return static_cast<int32_t>(value);
    }

    std::int32_t GetInt32BE()
    {
        uint32_t value = 0;
        value |= bytes[cursor++] << 24;
        value |= bytes[cursor++] << 16;
        value |= bytes[cursor++] << 8;
        value |= bytes[cursor++];
        return static_cast<int32_t>(value);
    }

    template<typename T>
    T GetObj()
    {
        T obj{};
        memcpy((void*)&obj, &bytes[cursor], sizeof(T));
        cursor += sizeof(T);
        return obj;
    }

    void GetBytes(void* dest, std::size_t size)
    {
        memcpy((void*)dest, &bytes[cursor], size);
        cursor += size;
        // eastl::copy(&bytes[cursor], &bytes[cursor + size], (std::uint8_t*)dest);
    }
};

};
