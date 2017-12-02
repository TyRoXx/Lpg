#pragma once

#include "lpg_check.h"
#include "lpg_local_variable.h"

typedef struct function_checking_state
{
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
} function_checking_state;