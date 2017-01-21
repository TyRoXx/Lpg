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

void *allocate_array(size_t size, size_t element)
{
    void *memory = calloc(size, element);
    if (!memory)
    {
        abort();
    }
    ++active_allocations;
    return memory;
}

void *reallocate(void *memory, size_t new_size)
{
    int const was_null = (memory == NULL);
    void *const new_memory = realloc(memory, new_size);
    if (!new_memory)
    {
        --active_allocations;
        return NULL;
    }
    if (was_null)
    {
        ++active_allocations;
    }
    return new_memory;
}

void *reallocate_array(void *memory, size_t new_size, size_t element)
{
    int const was_null = (memory == NULL);
    void *const new_memory = _recalloc(memory, new_size, element);
    if (!new_memory)
    {
        --active_allocations;
        return NULL;
    }
    if (was_null)
    {
        ++active_allocations;
    }
    return new_memory;
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
