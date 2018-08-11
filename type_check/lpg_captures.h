#pragma once

#include "stdbool.h"
#include "lpg_struct_member_id.h"
#include "lpg_register.h"
#include "lpg_type.h"

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

typedef struct capture
{
    variable_address from;
    type what;
} capture;

capture capture_create(variable_address const from, type const what);

variable_address variable_address_from_capture(capture_index const captured);

bool variable_address_equals(variable_address const left, variable_address const right);

struct function_checking_state;
capture_index require_capture(LPG_NON_NULL(struct function_checking_state *const state),
                              variable_address const captured, type const what);
