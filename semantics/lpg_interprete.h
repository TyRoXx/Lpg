#pragma once
#include "lpg_value.h"
#include "lpg_checked_program.h"

value call_function(function_pointer_value const callee,
                    value const *const inferred, value *const arguments,
                    value const *globals, garbage_collector *const gc,
                    checked_function const *const all_functions);

void interprete(checked_program const program, value const *globals,
                garbage_collector *const gc);
