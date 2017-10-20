#include "lpg_garbage_collector.h"
#include "lpg_allocate.h"
#include "lpg_stream_writer.h"
#include "lpg_assert.h"

void *garbage_collector_allocate(garbage_collector *const gc, size_t const bytes)
{
    return garbage_collector_allocate_with_destructor(gc, bytes, NULL);
}

void *garbage_collector_allocate_with_destructor(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes,
                                                 garbage_collector_destructor const destructor)
{
    memory_allocation *const allocation = allocate(sizeof(*allocation) + bytes);
    allocation->next = gc->allocations;
    allocation->destructor = destructor;
    gc->allocations = allocation;
    return &allocation->payload;
}

void *garbage_collector_allocate_array(garbage_collector *const gc, size_t const length, size_t const element)
{
    optional_size const bytes = size_multiply(length, element);
    ASSERT(bytes.state == optional_set);
    return garbage_collector_allocate(gc, bytes.value_if_set);
}

void garbage_collector_free(garbage_collector const gc)
{
    memory_allocation *i = gc.allocations;
    while (i)
    {
        memory_allocation *const next = i->next;
        if (i->destructor)
        {
            i->destructor(i->payload);
        }
        deallocate(i);
        i = next;
    }
}
