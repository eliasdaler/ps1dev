#pragma once

#include <cstdint>
#include <string.h>
#include <utility>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/span.h>

#undef ONE
#include <psyqo/gte-registers.hh>
#define ONE 4096

namespace util
{

template<unsigned... regs>
inline void clearAllGTERegistersInternal(std::integer_sequence<unsigned, regs...> regSeq)
{
    ((psyqo::GTE::clear<psyqo::GTE::Register{regs}, psyqo::GTE::Safety::Safe>()), ...);
}

inline void clearAllGTERegisters()
{
    clearAllGTERegistersInternal(std::make_integer_sequence<unsigned, 64>{});
}

eastl::vector<uint8_t> readFile(eastl::string_view filename);

};
