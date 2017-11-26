#pragma once
#include "lpg_checked_function.h"
#include "lpg_garbage_collector.h"
#include "lpg_function_id.h"
#include "lpg_interface_id.h"
#include "lpg_struct_id.h"

typedef struct checked_program
{
    interface *interfaces;
    interface_id interface_count;
    structure *structs;
    struct_id struct_count;
    garbage_collector memory;
    checked_function *functions;
    function_id function_count;
} checked_program;

void checked_program_free(LPG_NON_NULL(checked_program const *program));
