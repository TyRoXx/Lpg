#pragma once
#include <stdlib.h>
#include <stdio.h>

#define ASSERT(x)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            abort();                                                                                                   \
        }                                                                                                              \
    } while ((void)0, 0)

#ifdef NDEBUG
#ifdef _MSC_VER
#define ASSUME(x) ASSERT((x))
#else
#define ASSUME(x)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            __builtin_unreachable();                                                                                   \
        }                                                                                                              \
    } while ((void)0, 0)
#endif
#else
#define ASSUME(x) ASSERT(x)
#endif

#ifdef NDEBUG
#ifdef _MSC_VER
#define LPG_UNREACHABLE() ASSUME(0)
#else
#define LPG_UNREACHABLE() __builtin_unreachable()
#endif
#else
#define LPG_UNREACHABLE() abort()
#endif

#ifdef _MSC_VER
#define LPG_NO_RETURN __declspec(noreturn)
#else
#define LPG_NO_RETURN __attribute__((noreturn))
#endif

LPG_NO_RETURN void lpg_to_do(char const *const file, size_t const line);

#define LPG_TO_DO() lpg_to_do(__FILE__, __LINE__)

#define LPG_STATIC_ASSERT_IMPL3(X, L)                                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        lpg_static_assertion_##L = sizeof(char[1 - (2 * !(X))])                                                        \
    };
#define LPG_STATIC_ASSERT_IMPL2(X, L) LPG_STATIC_ASSERT_IMPL3(X, L)
#define LPG_STATIC_ASSERT(X) LPG_STATIC_ASSERT_IMPL2(X, __LINE__)
