#pragma once

#include "lpg_local_variable.h"
#include "lpg_program_check.h"
#include "lpg_semantic_error.h"
#include "lpg_checked_program.h"

typedef void check_error_handler(semantic_error, void *);

typedef struct function_checking_state
{
    program_check *root;
    struct function_checking_state *parent;
    bool may_capture_runtime_variables;
    struct capture *captures;
    capture_index capture_count;
    register_id used_registers;
    unicode_string *register_debug_names;
    register_id register_debug_name_count;
    bool is_in_loop;
    structure const *global;
    check_error_handler *on_error;
    local_variable_container local_variables;
    void *user;
    checked_program *program;
    instruction_sequence *body;
    optional_type return_type;
    bool has_declared_return_type;
    optional_value *register_compile_time_values;
    register_id register_compile_time_value_count;
} function_checking_state;

optional_value read_register_compile_time_value(function_checking_state const *state, register_id const which);
void write_register_compile_time_value(function_checking_state *state, register_id const which,
                                       value const compile_time_value);
