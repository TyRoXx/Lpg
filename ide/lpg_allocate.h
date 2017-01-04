#pragma once
#include <stddef.h>

void *allocate(size_t size);
void deallocate(void *memory);
void check_allocations(void);
