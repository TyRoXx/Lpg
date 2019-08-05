#pragma once
#include "lpg_captures.h"
#include "lpg_register.h"
#include "lpg_source_location.h"
#include "lpg_value.h"

typedef enum local_variable_phase {
    local_variable_phase_declared = 1,
    local_variable_phase_early_initialized = 2,
    local_variable_phase_lambda_being_checked = 3,
    local_variable_phase_initialized = 4
} local_variable_phase;

typedef struct local_variable
{
    unicode_view name;
    local_variable_phase phase;
    type type_;
    optional_value compile_time_value;
    register_id where;
    struct function_checking_state *lambda_origin;
} local_variable;

local_variable local_variable_create(unicode_view name, local_variable_phase phase, type const type_,
                                     optional_value compile_time_value, register_id where,
                                     struct function_checking_state *lambda_origin);

void local_variable_free(local_variable const *const freed);

typedef struct local_variable_container
{
    local_variable *elements;
    size_t count;
} local_variable_container;

void local_variable_container_free(local_variable_container const freed);

typedef enum read_local_variable_status {
    read_local_variable_status_at_address = 1,
    read_local_variable_status_compile_time_value,
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

static read_local_variable_result const read_local_variable_result_unknown = {read_local_variable_status_unknown,
                                                                              {{false, 0}, 0},
                                                                              {type_kind_unit, {0}},
                                                                              {false, {value_kind_unit, {{NULL, 0}}}},
                                                                              false};

static read_local_variable_result const read_local_variable_result_forbidden = {read_local_variable_status_forbidden,
                                                                                {{false, 0}, 0},
                                                                                {type_kind_unit, {0}},
                                                                                {false, {value_kind_unit, {{NULL, 0}}}},
                                                                                false};

read_local_variable_result read_local_variable_result_create(read_local_variable_status status, variable_address where,
                                                             type what, optional_value compile_time_value,
                                                             bool is_pure);

struct function_checking_state;
read_local_variable_result read_local_variable(LPG_NON_NULL(struct function_checking_state *const state),
                                               unicode_view const name,
                                               source_location const original_reference_location);

void add_local_variable(local_variable_container *to, local_variable variable);

void local_variable_initialize(local_variable_container *variables, unicode_view name, type what,
                               optional_value compile_time_value, register_id where);

void initialize_early(local_variable_container *variables, unicode_view name, type what,
                      optional_value compile_time_value, register_id where);

void initialize_lambda_being_checked(local_variable_container *variables, unicode_view name, type const what,
                                     struct function_checking_state *lambda_origin);

bool local_variable_name_exists(local_variable_container const variables, unicode_view const name);

variable_address variable_address_from_local(register_id const local);
