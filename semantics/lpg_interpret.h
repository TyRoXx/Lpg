#pragma once
#include "lpg_value.h"
#include "lpg_checked_program.h"

optional_value call_function(function_pointer_value const callee, function_call_arguments const arguments);

void interpret(checked_program const program, value const *globals, LPG_NON_NULL(garbage_collector *const gc));
