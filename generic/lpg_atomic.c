#include "lpg_atomic.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif

#ifdef _MSC_VER
void atomic_size_increment(size_t *atomic)
{
    InterlockedIncrementSizeT(atomic);
}

void atomic_size_decrement(size_t *atomic)
{
    InterlockedDecrementSizeT(atomic);
}

size_t atomic_size_load(size_t *atomic)
{
    return *(size_t volatile *)atomic;
}
#else
void atomic_size_increment(size_t *atomic)
{
    __atomic_add_fetch(atomic, 1, __ATOMIC_RELAXED);
}

void atomic_size_decrement(size_t *atomic)
{
    __atomic_sub_fetch(atomic, 1, __ATOMIC_RELAXED);
}

size_t atomic_size_load(size_t *atomic)
{
    size_t result;
    __atomic_load(atomic, &result, __ATOMIC_RELAXED);
    return result;
}
#endif
