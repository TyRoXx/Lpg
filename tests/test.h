#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#if defined(NDEBUG) || !defined(_MSC_VER)
#define LPG_ABORT() abort()
#else
#define LPG_ABORT()                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        __debugbreak();                                                                                                \
        abort();                                                                                                       \
    } while ((void)0, 0)
#endif

bool lpg_check(bool const success);
int lpg_print_test_summary(void);

#ifdef NDEBUG
#define FAIL() exit(1)
#else
#define FAIL() LPG_ABORT()
#endif

#define LPG_STRINGIZE(a) #a

#define REQUIRE(x)                                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        bool const lpg_require_local_variable = !!(x);                                                                 \
        lpg_check(lpg_require_local_variable);                                                                         \
        if (!lpg_require_local_variable)                                                                               \
        {                                                                                                              \
            fprintf(stderr, "REQUIRE(" LPG_STRINGIZE(x) ") failed\n");                                                 \
            FAIL();                                                                                                    \
        }                                                                                                              \
    } while ((void)0, 0)

#define REQUIRE_WITH_MESSAGE(x, ...)                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        bool const lpg_require_local_variable = !!(x);                                                                 \
        lpg_check(lpg_require_local_variable);                                                                         \
        if (!lpg_require_local_variable)                                                                               \
        {                                                                                                              \
            fprintf(stderr, "REQUIRE(" LPG_STRINGIZE(x) ") failed\n");                                                 \
            fprintf(stderr, __VA_ARGS__);                                                                              \
            FAIL();                                                                                                    \
        }                                                                                                              \
    } while ((void)0, 0)
