#pragma once
#include "lpg_checked_function.h"
#include "lpg_garbage_collector.h"

typedef struct checked_program
{
    garbage_collector memory;
    checked_function *functions;
    size_t function_count;
} checked_program;

void checked_program_free(checked_program const *program);
