#pragma once

#include "lpg_parse_expression.h"
#include "lpg_source_file.h"
#include "lpg_value.h"

typedef struct complete_parse_error
{
    parse_error relative;
    unicode_view file_name;
    unicode_view source;
    source_file_lines lines;
} complete_parse_error;

complete_parse_error complete_parse_error_create(parse_error relative, unicode_view file_name, unicode_view source,
                                                 source_file_lines lines);
bool complete_parse_error_equals(complete_parse_error const left, complete_parse_error const right);

typedef void complete_parse_error_handler(complete_parse_error, callback_user);

typedef struct module_loader
{
    unicode_view module_directory;
    complete_parse_error_handler *on_parse_error;
    callback_user on_parse_error_user;
} module_loader;

module_loader module_loader_create(unicode_view module_directory, complete_parse_error_handler on_parse_error,
                                   callback_user on_parse_error_user);

typedef struct load_module_result
{
    optional_value loaded;
    type schema;
} load_module_result;

struct function_checking_state;

load_module_result load_module(LPG_NON_NULL(struct function_checking_state *const state), unicode_view const name);
