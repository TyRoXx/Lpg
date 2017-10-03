#pragma once
#include "lpg_type.h"
#include "lpg_value.h"

struct structure_member
{
    type what;
    unicode_string name;
    optional_value compile_time_value;
};

structure_member structure_member_create(type what, unicode_string name, optional_value compile_time_value);
void struct_member_free(LPG_NON_NULL(structure_member const *value));
