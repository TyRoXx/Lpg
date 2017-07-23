#pragma once
#include "lpg_array_size.h"
#include <stddef.h>
#include "lpg_non_null.h"

void *allocate(size_t size);
void *allocate_array(size_t size, size_t element);
void *reallocate(void *memory, size_t new_size);
void *reallocate_array(void *memory, size_t new_size, size_t element);
void deallocate(LPG_NON_NULL(void *memory));
size_t count_total_allocations(void);
size_t count_active_allocations(void);

void *copy_array_impl(LPG_NON_NULL(void const *from), size_t size_in_bytes);

#define LPG_COPY_ARRAY(array)                                                  \
    copy_array_impl((array), sizeof(array)), LPG_ARRAY_SIZE(array)
