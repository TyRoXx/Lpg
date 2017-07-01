#pragma once
#include "lpg_checked_function.h"
#include "lpg_garbage_collector.h"
#include "lpg_function_id.h"

typedef struct checked_program
{
    garbage_collector memory;
    checked_function *functions;
    function_id function_count;
} checked_program;

void checked_program_free(checked_program const *program);
