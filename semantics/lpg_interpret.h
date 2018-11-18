#pragma once
#include "lpg_checked_program.h"
#include "lpg_optional_function_id.h"
#include "lpg_value.h"

external_function_result call_function(function_pointer_value const callee, function_call_arguments const arguments);

external_function_result call_checked_function(checked_function const callee, optional_function_id const callee_id,
                                               value const *const captures, function_call_arguments arguments);

void interpret(checked_program const program, value const *globals, LPG_NON_NULL(garbage_collector *const gc));
