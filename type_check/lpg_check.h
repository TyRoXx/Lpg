#pragma once
#include "lpg_parse_expression.h"
#include "lpg_checked_program.h"
#include "lpg_semantic_error.h"
#include "lpg_program_check.h"
#include "lpg_load_module.h"
#include "lpg_function_checking_state.h"
#include "lpg_optional_function_id.h"

typedef struct check_function_result
{
    bool success;
    checked_function function;
    capture *captures;
    size_t capture_count;
} check_function_result;

check_function_result check_function_result_create(checked_function function, capture *captures, size_t capture_count);

extern check_function_result const check_function_result_empty;

check_function_result
check_function(program_check *const root, function_checking_state *const parent, expression const body_in,
               structure const global, check_error_handler *const on_error, void *const user,
               checked_program *const program, type const *const parameter_types, unicode_string *const parameter_names,
               size_t const parameter_count, optional_type const self, bool const may_capture_runtime_variables,
               optional_type const explicit_return_type, unicode_view const file_name, unicode_view const source,
               unicode_view const *const early_initialized_variable, optional_function_id const current_function_id);

checked_program check(sequence const root, structure const global, LPG_NON_NULL(check_error_handler *on_error),
                      LPG_NON_NULL(module_loader *loader), unicode_view file_name, unicode_view source, void *user);
