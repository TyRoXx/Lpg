#include "lpg_garbage_collector.h"
#include "lpg_allocate.h"

void *garbage_collector_allocate(garbage_collector *const gc,
                                 size_t const bytes)
{
    memory_allocation *const allocation = allocate(sizeof(*allocation) + bytes);
    allocation->next = gc->allocations;
    gc->allocations = allocation;
    return &allocation->payload;
}

void garbage_collector_free(garbage_collector const gc)
{
    memory_allocation *i = gc.allocations;
    while (i)
    {
        memory_allocation *const next = i->next;
        deallocate(i);
        i = next;
    }
}
