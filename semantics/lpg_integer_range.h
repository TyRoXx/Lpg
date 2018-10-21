#pragma once

#include "lpg_integer.h"

typedef struct integer_range
{
    integer minimum;
    integer maximum;
} integer_range;

typedef struct integer_range_list
{
    integer_range *elements;
    size_t length;
} integer_range_list;

integer_range integer_range_create(integer const minimum, integer const maximum);
integer_range integer_range_max(void);
bool integer_range_equals(integer_range const left, integer_range const right);
bool integer_range_contains(integer_range const haystack, integer_range const needle);
bool integer_range_contains_integer(integer_range const haystack, integer const needle);
integer_range integer_range_combine(integer_range const left, integer_range const right);

integer_range_list integer_range_list_create_from_integer_range(integer_range const element);
integer_range_list integer_range_list_create(integer_range *elements, size_t const length);
void integer_range_list_deallocate(integer_range_list value);

bool integer_range_list_contains(integer_range_list const haystack, integer_range const needle);
void integer_range_list_merge(integer_range_list *const unmerged_list);
void integer_range_list_remove(integer_range_list *const haystack, integer_range const needle);
integer integer_range_list_size(integer_range_list const list);
integer integer_range_size(integer_range const value);
