#include "lpg_garbage_collector.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_stream_writer.h"

enum
{
    block_size = 1024 * 10,
    alignment = 16
};

garbage_collector garbage_collector_create(size_t max_heap_size)
{
    garbage_collector const result = {max_heap_size, 0, NULL};
    return result;
}

void *garbage_collector_try_allocate(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes)
{
    if (bytes == 0)
    {
        return NULL;
    }
    optional_size const new_heap_size = size_add(gc->current_heap_size, bytes);
    if (new_heap_size.state == optional_empty)
    {
        return NULL;
    }
    if (new_heap_size.value_if_set > gc->max_heap_size)
    {
        return NULL;
    }
    {
        size_t const remaining_in_block = gc->blocks ? (gc->blocks->size - gc->blocks->used) : 0u;
        if (remaining_in_block < bytes)
        {
            size_t new_block_size = block_size;
            if (bytes > new_block_size)
            {
                new_block_size = bytes;
            }
            memory_block *const new_block = allocate(sizeof(*new_block) + new_block_size);
            new_block->next = gc->blocks;
            new_block->used = 0;
            new_block->size = new_block_size;
            gc->blocks = new_block;
        }
    }
    void *const result = (gc->blocks->data + gc->blocks->used);
    gc->blocks->used += bytes;
    gc->blocks->used += (alignment - 1u);
    gc->blocks->used &= ~(alignment - 1u);
    ASSUME((gc->blocks->used % alignment) == 0);
    if (gc->blocks->used > gc->blocks->size)
    {
        gc->blocks->used = gc->blocks->size;
    }
    gc->current_heap_size = new_heap_size.value_if_set;
    return result;
}

void *garbage_collector_allocate(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes)
{
    void *const result = garbage_collector_try_allocate(gc, bytes);
    ASSERT((bytes == 0) || result);
    return result;
}

void *garbage_collector_try_allocate_array(LPG_NON_NULL(garbage_collector *const gc), size_t const length,
                                           size_t const element)
{
    optional_size const bytes = size_multiply(length, element);
    ASSERT(bytes.state == optional_set);
    return garbage_collector_try_allocate(gc, bytes.value_if_set);
}

void *garbage_collector_allocate_array(LPG_NON_NULL(garbage_collector *const gc), size_t const length,
                                       size_t const element)
{
    void *const result = garbage_collector_try_allocate_array(gc, length, element);
    ASSERT((length == 0) || (element == 0) || result);
    return result;
}

void garbage_collector_free(garbage_collector const gc)
{
    memory_block *i = gc.blocks;
    while (i)
    {
        memory_block *const next = i->next;
        deallocate(i);
        i = next;
    }
}
