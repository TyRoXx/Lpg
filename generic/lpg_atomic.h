#pragma once
#include <stddef.h>

void atomic_size_increment(size_t *atomic);
void atomic_size_decrement(size_t *atomic);
size_t atomic_size_load(size_t *atomic);
