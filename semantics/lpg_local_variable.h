#pragma once
#include "lpg_register.h"
#include "lpg_value.h"

typedef struct local_variable
{
    unicode_string name;
    type type_;
    optional_value compile_time_value;
    register_id where;
} local_variable;

local_variable local_variable_create(unicode_string name, type const type_, optional_value compile_time_value,
                                     register_id where);

void local_variable_free(local_variable const *const value);

typedef struct local_variable_container
{
    local_variable *elements;
    size_t count;
} local_variable_container;

typedef enum read_local_variable_status
{
    read_local_variable_status_ok,
    read_local_variable_status_unknown,
    read_local_variable_status_forbidden
} read_local_variable_status;

typedef struct optional_capture_index
{
    bool has_value;
    capture_index value;
} optional_capture_index;

static optional_capture_index const optional_capture_index_empty = {false, 0};

typedef struct variable_address
{
    optional_capture_index captured_in_current_lambda;

    /*set if captured_in_current_lambda is empty:*/
    register_id local_address;
} variable_address;

typedef struct read_local_variable_result
{
    /*if status is ok, the other members are set*/
    read_local_variable_status status;
    variable_address where;
    type what;
    optional_value compile_time_value;
    bool is_pure;
} read_local_variable_result;

read_local_variable_result read_local_variable_result_create(variable_address where, type what,
                                                             optional_value compile_time_value, bool is_pure);

void add_local_variable(local_variable_container *to, local_variable variable);

bool local_variable_name_exists(local_variable_container const variables, unicode_view const name);
