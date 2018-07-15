#pragma once
#include <stddef.h>

#ifdef _WIN32
#include <Windows.h>
inline void atomic_size_increment(size_t *atomic)
{
    InterlockedIncrementSizeT(atomic);
}

inline void atomic_size_decrement(size_t *atomic)
{
    InterlockedDecrementSizeT(atomic);
}

inline size_t atomic_size_load(size_t *atomic)
{
    return *(size_t volatile *)atomic;
}
#else
static inline void atomic_size_increment(size_t *atomic)
{
    __atomic_add_fetch(atomic, 1, __ATOMIC_RELAXED);
}

static inline void atomic_size_decrement(size_t *atomic)
{
    __atomic_sub_fetch(atomic, 1, __ATOMIC_RELAXED);
}

static inline size_t atomic_size_load(size_t *atomic)
{
    size_t result;
    __atomic_load(atomic, &result, __ATOMIC_RELAXED);
    return result;
}
#endif
