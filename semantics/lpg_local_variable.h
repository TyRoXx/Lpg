#pragma once
#include "lpg_register_id.h"
#include "lpg_value.h"

typedef struct local_variable
{
    unicode_string name;
    type type_;
    optional_value compile_time_value;
    register_id where;
} local_variable;

local_variable local_variable_create(unicode_string name, type const type_,
                                     optional_value compile_time_value,
                                     register_id where);

void local_variable_free(local_variable const *const value);

typedef struct local_variable_container
{
    local_variable *elements;
    size_t count;
} local_variable_container;

void add_local_variable(local_variable_container *to, local_variable variable);

bool local_variable_name_exists(local_variable_container const variables,
                                unicode_view const name);
