#pragma once
#include "lpg_assert.h"
#include "lpg_unicode_view.h"
#include "math.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct integer
{
    uint64_t high;
    uint64_t low;
} integer;

integer integer_create(uint64_t high, uint64_t low) LPG_USE_RESULT;
integer integer_max(void) LPG_USE_RESULT;

integer integer_shift_left_truncate(integer value, uint32_t bits) LPG_USE_RESULT;
bool integer_shift_left(LPG_NON_NULL(integer *value), uint32_t bits) LPG_USE_RESULT;

bool integer_bit(integer value, uint32_t bit) LPG_USE_RESULT;
void integer_set_bit(LPG_NON_NULL(integer *target), uint32_t bit, bool value);

bool integer_equal(integer left, integer right) LPG_USE_RESULT;
bool integer_less(integer left, integer right) LPG_USE_RESULT;
bool integer_less_or_equals(integer left, integer right) LPG_USE_RESULT;

integer integer_maximum(integer const first, integer const second);
integer integer_minimum(integer const first, integer const second);

bool integer_add(LPG_NON_NULL(integer *left), integer right) LPG_USE_RESULT;
integer integer_subtract(integer minuend, integer subtrahend) LPG_USE_RESULT;
bool integer_multiply(LPG_NON_NULL(integer *left), integer right) LPG_USE_RESULT;

bool integer_parse(LPG_NON_NULL(integer *into), unicode_view from) LPG_USE_RESULT;

typedef struct integer_division
{
    integer quotient;
    integer remainder;
} integer_division;

integer_division integer_divide(integer numerator, integer denominator) LPG_USE_RESULT;

extern char const lower_case_digits[];

unicode_view integer_format(integer const value, LPG_NON_NULL(char const *digits), unsigned base,
                            LPG_NON_NULL(char *buffer), size_t buffer_size) LPG_USE_RESULT;
