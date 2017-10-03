#pragma once
#include <stddef.h>
#include <lpg_non_null.h>

typedef struct memory_allocation
{
    struct memory_allocation *next;
    char payload[
#ifdef _MSC_VER
        1
#endif
    ];
} memory_allocation;

typedef struct garbage_collector
{
    memory_allocation *allocations;
} garbage_collector;

void *garbage_collector_allocate(LPG_NON_NULL(garbage_collector *const gc), size_t const bytes);

void *garbage_collector_allocate_array(LPG_NON_NULL(garbage_collector *const gc), size_t const length,
                                       size_t const element);

void garbage_collector_free(garbage_collector const gc);
