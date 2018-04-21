#pragma once

#include "lpg_value.h"
#include "lpg_parse_expression.h"

typedef struct complete_parse_error
{
    parse_error relative;
    unicode_view file_name;
    unicode_view source;
} complete_parse_error;

complete_parse_error complete_parse_error_create(parse_error relative, unicode_view file_name, unicode_view source);
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

load_module_result load_module(LPG_NON_NULL(module_loader *loader), unicode_view name);
