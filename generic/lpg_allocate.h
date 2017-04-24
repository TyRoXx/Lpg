#pragma once
#include <stddef.h>

void *allocate(size_t size);
void *allocate_array(size_t size, size_t element);
void *reallocate(void *memory, size_t new_size);
void *reallocate_array(void *memory, size_t new_size, size_t element);
void deallocate(void *memory);
size_t count_total_allocations(void);
