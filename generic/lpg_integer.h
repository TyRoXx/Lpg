#pragma once
#include <stdint.h>
#include <stddef.h>
#include "lpg_unicode_view.h"
#include <stdbool.h>

typedef struct integer
{
    uint64_t high;
    uint64_t low;
} integer;

integer integer_create(uint64_t high, uint64_t low);
integer integer_shift_left_truncate(integer value, uint32_t bits);
bool integer_shift_left(integer *value, uint32_t bits);
bool integer_bit(integer value, uint32_t bit);
void integer_set_bit(integer *target, uint32_t bit, bool value);
bool integer_equal(integer left, integer right);
bool integer_less(integer left, integer right);
integer integer_subtract(integer minuend, integer subtrahend);
bool integer_multiply(integer *left, integer right);
bool integer_add(integer *left, integer right);
bool integer_parse(integer *into, unicode_view from);

typedef struct integer_division
{
    integer quotient;
    integer remainder;
} integer_division;

integer_division integer_divide(integer numerator, integer denominator);

extern char const lower_case_digits[];

char *integer_format(integer const value, char const *digits, unsigned base,
                     char *buffer, size_t buffer_size);
