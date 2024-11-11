#pragma once

#include <EASTL/fixed_string.h>
#include <EASTL/string_view.h>

#include <common/syscalls/syscalls.h>
#include <psyqo/kernel.hh>
#include <psyqo/matrix.hh>
#include <psyqo/xprintf.h>

namespace testutil
{
inline void printMatrix(eastl::string_view matrixName, const psyqo::Matrix33& m)
{
    eastl::fixed_string<char, 128> str;
    str.clear();
    sprintf(
        str.data(),
        "%s =\n\t(%f, %f, %f)\n\t(%f, %f, %f)\n\t(%f, %f, %f)",
        matrixName.data(),
        m.vs[0].x,
        m.vs[0].y,
        m.vs[0].z,
        m.vs[1].x,
        m.vs[1].y,
        m.vs[1].z,
        m.vs[2].x,
        m.vs[2].y,
        m.vs[2].z);
    ramsyscall_printf("%s\n", str.c_str());
};

inline void printSeparatorLine()
{
    ramsyscall_printf("---------------------------\n");
}

inline void printVec3(eastl::string_view vecName, const psyqo::Vec3& v)
{
    eastl::fixed_string<char, 128> str;
    str.clear();
    sprintf(str.data(), "%s =\n\t(%f, %f, %f)", vecName.data(), v.x, v.y, v.z);
    ramsyscall_printf("%s\n", str.c_str());
}

inline void assertF(bool condition, std::source_location loc, const char* fmt, ...)
{
    static constexpr auto MSG_LENGTH_MAX = 512;
    static eastl::fixed_string<char, MSG_LENGTH_MAX> msgString;
    if (!condition) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(msgString.data(), MSG_LENGTH_MAX, fmt, args);
        va_end(args);
        psyqo::Kernel::abort(msgString.data(), loc);
    }
}

#define ASSERT_EQFP(expected, actual, msg)               \
    assertF(                                             \
        expected == actual,                              \
        std::source_location::current(),                 \
        "comparison failed: %s\n\t%s != %s\n\t%f != %f", \
        msg,                                             \
        #expected,                                       \
        #actual,                                         \
        expected,                                        \
        actual);

inline void assertMatrixEqual(const psyqo::Matrix33& expected, const psyqo::Matrix33& actual)
{
    using namespace psyqo::Kernel;
    ASSERT_EQFP(expected.vs[0].x, actual.vs[0].x, "e_11 != a_11");
    ASSERT_EQFP(expected.vs[0].y, actual.vs[0].y, "e_12 != a_12");
    ASSERT_EQFP(expected.vs[0].z, actual.vs[0].z, "e_13 != a_13");
    ASSERT_EQFP(expected.vs[1].x, actual.vs[1].x, "e_21 != a_21");
    ASSERT_EQFP(expected.vs[1].y, actual.vs[1].y, "e_22 != a_22");
    ASSERT_EQFP(expected.vs[1].z, actual.vs[1].z, "e_23 != a_23");
    ASSERT_EQFP(expected.vs[2].x, actual.vs[2].x, "e_31 != a_31");
    ASSERT_EQFP(expected.vs[2].y, actual.vs[2].y, "e_32 != a_32");
    ASSERT_EQFP(expected.vs[2].z, actual.vs[2].z, "e_33 != a_33");
};

inline void assertVec3Equal(const psyqo::Vec3& expected, const psyqo::Vec3& actual)
{
    using namespace psyqo::Kernel;
    ASSERT_EQFP(expected.x, actual.x, "e.x != a.x");
    ASSERT_EQFP(expected.y, actual.y, "e.y != a.y");
    ASSERT_EQFP(expected.z, actual.z, "e.z != a.z");
}

} // end of namespace testutil
