#pragma once
#include "lpg_register.h"
#include "lpg_value.h"
#include "lpg_captures.h"

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

typedef struct read_local_variable_result
{
    /*if status is ok, the other members are set*/
    read_local_variable_status status;
    variable_address where;
    type what;
    optional_value compile_time_value;
    bool is_pure;
} read_local_variable_result;

static read_local_variable_result const read_local_variable_result_unknown = {
    read_local_variable_status_unknown, {{false, 0}, 0}, {type_kind_unit, {0}}, {false, {value_kind_unit, {0}}}, false};

static read_local_variable_result const read_local_variable_result_forbidden = {read_local_variable_status_forbidden,
                                                                                {{false, 0}, 0},
                                                                                {type_kind_unit, {0}},
                                                                                {false, {value_kind_unit, {0}}},
                                                                                false};

read_local_variable_result read_local_variable_result_create(variable_address where, type what,
                                                             optional_value compile_time_value, bool is_pure);

struct function_checking_state;
read_local_variable_result read_local_variable(LPG_NON_NULL(struct function_checking_state *const state),
                                               instruction_sequence *const sequence, unicode_view const name,
                                               source_location const original_reference_location);

void add_local_variable(local_variable_container *to, local_variable variable);

bool local_variable_name_exists(local_variable_container const variables, unicode_view const name);

variable_address variable_address_from_local(register_id const local);