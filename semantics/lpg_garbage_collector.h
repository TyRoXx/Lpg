#pragma once
#include <stddef.h>
#include <lpg_non_null.h>

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
    memory_block *blocks;
} garbage_collector;

void *garbage_collector_allocate(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes);

void *garbage_collector_allocate_array(LPG_NON_NULL(garbage_collector *const gc), size_t const length,
                                       size_t const element);

void garbage_collector_free(garbage_collector const gc);
