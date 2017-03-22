#pragma once

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#if defined(NDEBUG) || !defined(_MSC_VER)
#define REQUIRE(x)                                                             \
    if (!(x))                                                                  \
    {                                                                          \
        abort();                                                               \
    }
#else
#define REQUIRE(x)                                                             \
    if (!(x))                                                                  \
    {                                                                          \
        __debugbreak();                                                        \
    }
#endif
