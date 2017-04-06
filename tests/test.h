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

#define REQUIRE(x)                                                             \
    if (!(x))                                                                  \
    {                                                                          \
        LPG_ABORT();                                                           \
    }
