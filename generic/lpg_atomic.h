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
void atomic_size_increment(size_t *atomic);
void atomic_size_decrement(size_t *atomic);
size_t atomic_size_load(size_t *atomic);
#endif
