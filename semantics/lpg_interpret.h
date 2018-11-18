#pragma once
#include "lpg_checked_program.h"
#include "lpg_optional_function_id.h"
#include "lpg_value.h"

typedef struct interpreter
{
    value const *globals;
    garbage_collector *const gc;
    checked_function *const *all_functions;
    lpg_interface *const *all_interfaces;
    size_t max_recursion;
    size_t *current_recursion;
    uint64_t max_executed_instructions;
    uint64_t *executed_instructions;
} interpreter;

interpreter interpreter_create(value const *globals, garbage_collector *const gc,
                               checked_function *const *const all_functions, lpg_interface *const *const all_interfaces,
                               size_t max_recursion, size_t *current_recursion, uint64_t max_executed_instructions,
                               uint64_t *executed_instructions) LPG_USE_RESULT;

external_function_result call_function(function_pointer_value const callee, optional_value const self,
                                       value *const arguments, interpreter *const context);

external_function_result call_checked_function(checked_function const callee, optional_function_id const callee_id,
                                               value const *const captures, optional_value const self,
                                               value *const arguments, interpreter *const context);

void interpret(checked_program const program, value const *globals, LPG_NON_NULL(garbage_collector *const gc));
