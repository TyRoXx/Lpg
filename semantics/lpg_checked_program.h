#pragma once
#include "lpg_checked_function.h"

typedef struct checked_program
{
    checked_function *functions;
    size_t function_count;
} checked_program;

void checked_program_free(checked_program const *program);
