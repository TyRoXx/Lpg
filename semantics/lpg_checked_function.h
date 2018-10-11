#pragma once
#include "lpg_instruction_sequence.h"
#include "lpg_register.h"
#include "lpg_type.h"
#include "lpg_unicode_string.h"
#include <lpg_non_null.h>

typedef struct function_pointer function_pointer;

typedef struct checked_function
{
    function_pointer *signature;
    instruction_sequence body;
    unicode_string *register_debug_names;
    register_id number_of_registers;
} checked_function;

checked_function checked_function_create(LPG_NON_NULL(function_pointer *signature), instruction_sequence body,
                                         unicode_string *register_debug_names, register_id number_of_registers);
void checked_function_free(LPG_NON_NULL(checked_function const *function));

optional_type get_return_type(type const callee, checked_function const *const all_functions,
                              lpg_interface const *const all_interfaces);
