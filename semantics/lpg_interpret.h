#pragma once
#include "lpg_value.h"
#include "lpg_checked_program.h"

optional_value call_function(function_pointer_value const callee, value const *const inferred, value *const arguments,
                             value const *globals, LPG_NON_NULL(garbage_collector *const gc),
                             LPG_NON_NULL(checked_function const *const all_functions));

void interpret(checked_program const program, value const *globals, LPG_NON_NULL(garbage_collector *const gc));
