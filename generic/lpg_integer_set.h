#pragma once
#include "lpg_integer_range.h"

typedef struct integer_set
{
    integer *elements;
    size_t used;
} integer_set;

integer_set integer_set_from_range(integer_range const range);
void integer_set_free(integer_set const freed);
bool integer_set_contains_range(integer_set const set, integer_range const range);
bool integer_set_contains(integer_set const set, integer const element);
void integer_set_remove_range(integer_set *const set, integer_range const range);
void integer_set_remove(integer_set *const set, integer const element);
bool integer_set_is_empty(integer_set const set);
