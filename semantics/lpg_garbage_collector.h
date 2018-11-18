#pragma once
#include <lpg_non_null.h>
#include <stddef.h>

typedef void (*garbage_collector_destructor)(void *);

typedef struct memory_block
{
    struct memory_block *next;
    size_t used;
    size_t size;
    char data[
#ifdef _MSC_VER
        1
#endif
    ];
} memory_block;

typedef struct garbage_collector
{
    size_t max_heap_size;
    size_t current_heap_size;
    memory_block *blocks;
} garbage_collector;

garbage_collector garbage_collector_create(size_t max_heap_size);

void *garbage_collector_allocate(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes);
void *garbage_collector_try_allocate(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes);

void *garbage_collector_allocate_array(LPG_NON_NULL(garbage_collector *const gc), size_t const length,
                                       size_t const element);
void *garbage_collector_try_allocate_array(LPG_NON_NULL(garbage_collector *const gc), size_t const length,
                                           size_t const element);

void garbage_collector_free(garbage_collector const gc);
