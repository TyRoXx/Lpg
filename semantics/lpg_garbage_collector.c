#include "lpg_garbage_collector.h"
#include "lpg_allocate.h"
#include "lpg_stream_writer.h"
#include "lpg_assert.h"

enum
{
    block_size = 1024 * 10,
    alignment = 16
};

void *garbage_collector_allocate(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes)
{
    if (bytes == 0)
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
    return result;
}

void *garbage_collector_allocate_array(LPG_NON_NULL(garbage_collector *const gc), size_t const length,
                                       size_t const element)
{
    optional_size const bytes = size_multiply(length, element);
    ASSERT(bytes.state == optional_set);
    return garbage_collector_allocate(gc, bytes.value_if_set);
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
