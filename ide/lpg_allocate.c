#include "lpg_allocate.h"
#include <stdlib.h>
#include <assert.h>

static size_t active_allocations = 0;

void *allocate(size_t size)
{
    void *memory = malloc(size);
    if (!memory)
    {
        abort();
    }
    ++active_allocations;
    return memory;
}

void deallocate(void *memory)
{
    --active_allocations;
    free(memory);
}

void check_allocations()
{
    assert(active_allocations == 0);
}
