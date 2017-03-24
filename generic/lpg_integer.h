#pragma once
#include <stdint.h>

typedef struct integer
{
    uint64_t high;
    uint64_t low;
} integer;

integer integer_create(uint64_t high, uint64_t low);
integer integer_shift_left(integer value, uint32_t bits);
unsigned integer_bit(integer value, uint32_t bit);
void integer_set_bit(integer *target, uint32_t bit, unsigned value);
unsigned integer_equal(integer left, integer right);
unsigned integer_less(integer left, integer right);
integer integer_subtract(integer minuend, integer subtrahend);

typedef struct integer_division
{
    integer quotient;
    integer remainder;
} integer_division;

integer_division integer_divide(integer numerator, integer denominator);
