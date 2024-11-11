#pragma once

#include <cassert>
#include <cstdint>
#include <cstring> // memcpy
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

struct FileReader {
    std::span<const std::uint8_t> bytes;
    int cursor{0};

    template<typename T>
    bool hasData()
    {
        return cursor <= (bytes.size() - sizeof(T));
    };

    uint8_t GetUInt8()
    {
        assert(hasData<uint16_t>() && "GetUInt8: EOF");
        return static_cast<uint8_t>(bytes[cursor++]);
    }

    uint16_t GetUInt16()
    {
        assert(hasData<uint16_t>() && "GetUInt16: EOF");
        uint16_t value = 0;
        value |= bytes[cursor++];
        value |= bytes[cursor++] << 8;
        return static_cast<uint16_t>(value);
    }

    int32_t GetUInt32()
    {
        assert(hasData<uint32_t>() && "GetUInt32: EOF");
        uint32_t value = 0;
        value |= bytes[cursor++];
        value |= bytes[cursor++] << 8;
        value |= bytes[cursor++] << 16;
        value |= bytes[cursor++] << 24;
        return value;
    }

    int8_t GetInt8() { return static_cast<int8_t>(GetUInt8()); }
    int16_t GetInt16() { return static_cast<int16_t>(GetUInt16()); }
    int32_t GetInt32() { return static_cast<int32_t>(GetUInt32()); }

    int16_t GetInt16BE()
    {
        uint16_t value = 0;
        value |= bytes[cursor++] << 8;
        value |= bytes[cursor++];
        return static_cast<int16_t>(value);
    }

    int32_t GetInt32BE()
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
        assert(hasData<T>() && "GetObj: EOF");
        T obj{};
        memcpy((void*)&obj, &bytes[cursor], sizeof(T));
        cursor += sizeof(T);
        return obj;
    }
};

inline std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file " + path.string());
    }
    const auto fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<std::uint8_t> contents(fileSize);
    static_assert(sizeof(char) == sizeof(uint8_t));
    file.read(reinterpret_cast<char*>(contents.data()), fileSize);
    return contents;
}
