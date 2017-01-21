#pragma once
#include <stddef.h>

void *allocate(size_t size);
void *allocate_array(size_t size, size_t element);
void *reallocate(void *memory, size_t new_size);
void deallocate(void *memory);
void check_allocations(void);
