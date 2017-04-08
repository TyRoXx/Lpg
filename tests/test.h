#pragma once

#include <stdlib.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#if defined(NDEBUG) || !defined(_MSC_VER)
#define LPG_ABORT() abort()
#else
#define LPG_ABORT()                                                            \
    do                                                                         \
    {                                                                          \
        __debugbreak();                                                        \
        abort();                                                               \
    } while (0)
#endif

int lpg_check(int success);
int lpg_print_test_summary(void);

#define FAIL() LPG_ABORT()

#define REQUIRE(x)                                                             \
    if (!lpg_check(!!(x)))                                                     \
    {                                                                          \
        FAIL();                                                                \
    }
