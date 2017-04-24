#pragma once
#include <stdlib.h>

#define ASSERT(x)                                                              \
    do                                                                         \
    {                                                                          \
        if (!(x))                                                              \
        {                                                                      \
            abort();                                                           \
        }                                                                      \
    } while (0)

#ifdef NDEBUG
#ifdef _MSC_VER
#define ASSUME(x) __assume((x))
#else
#define ASSUME(x)                                                              \
    do                                                                         \
    {                                                                          \
        if (!(x))                                                              \
        {                                                                      \
            __builtin_unreachable();                                           \
        }                                                                      \
    } while (0)
#endif
#else
#define ASSUME(x) ASSERT(x)
#endif

#ifdef NDEBUG
#ifdef _MSC_VER
#define UNREACHABLE() ASSUME(0)
#else
#define UNREACHABLE() __builtin_unreachable()
#endif
#else
#define UNREACHABLE() ASSERT(0)
#endif

#define LPG_TO_DO() UNREACHABLE()
