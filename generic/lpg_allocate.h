#pragma once
#include "lpg_array_size.h"
#include <stddef.h>
#include "lpg_non_null.h"
#include "lpg_use_result.h"

LPG_USE_RESULT void *allocate(size_t size);
LPG_USE_RESULT void *allocate_array(size_t size, size_t element);
LPG_USE_RESULT void *reallocate(void *memory, size_t new_size);
LPG_USE_RESULT void *reallocate_array(void *memory, size_t new_size,
                                      size_t element);
LPG_USE_RESULT void deallocate(LPG_NON_NULL(void *memory));
LPG_USE_RESULT size_t count_total_allocations(void);
LPG_USE_RESULT size_t count_active_allocations(void);

LPG_USE_RESULT void *copy_array_impl(LPG_NON_NULL(void const *from),
                                     size_t size_in_bytes);

#define LPG_COPY_ARRAY(array)                                                  \
    copy_array_impl((array), sizeof(array)), LPG_ARRAY_SIZE(array)
