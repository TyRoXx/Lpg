#pragma once
#include "lpg_parse_expression.h"
#include "lpg_checked_program.h"
#include "lpg_semantic_error.h"
#include "lpg_program_check.h"
#include "lpg_load_module.h"
#include "lpg_function_checking_state.h"

typedef struct check_function_result
{
    bool success;
    checked_function function;
    capture *captures;
    size_t capture_count;
} check_function_result;

check_function_result check_function_result_create(checked_function function, capture *captures, size_t capture_count);

extern check_function_result const check_function_result_empty;

check_function_result check_function(program_check *const root, function_checking_state *parent,
                                     expression const body_in, structure const global, check_error_handler *on_error,
                                     void *user, checked_program *const program, type const *const parameter_types,
                                     unicode_string *const parameter_names, size_t const parameter_count,
                                     optional_type const self, bool const may_capture_runtime_variables,
                                     optional_type const explicit_return_type);

checked_program check(sequence const root, structure const global, LPG_NON_NULL(check_error_handler *on_error),
                      LPG_NON_NULL(module_loader *loader), void *user);
