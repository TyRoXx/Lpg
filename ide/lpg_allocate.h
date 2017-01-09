#pragma once
#include <stddef.h>

void *allocate(size_t size);
void *reallocate(void *memory, size_t new_size);
void deallocate(void *memory);
void check_allocations(void);
