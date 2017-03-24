#include "lpg_integer.h"
#include "lpg_assert.h"

integer integer_create(uint64_t high, uint64_t low)
{
    integer result = {high, low};
    return result;
}

integer integer_shift_left(integer value, uint32_t bits)
{
    if (bits == 0)
    {
        return value;
    }
    if (bits == 64)
    {
        integer result = {value.low, 0};
        return result;
    }
    integer result = {((value.high << bits) | ((value.low >> (64u - bits)) &
                                               ((uint64_t)-1) >> (64u - bits))),
                      (value.low << bits)};
    return result;
}

unsigned integer_bit(integer value, uint32_t bit)
{
    if (bit < 64u)
    {
        return (value.low >> bit) & 1u;
    }
    return (value.high >> (bit - 64u)) & 1u;
}

void integer_set_bit(integer *target, uint32_t bit, unsigned value)
{
    if (bit < 64u)
    {
        target->low |= ((uint64_t)value << bit);
    }
    else
    {
        target->high |= ((uint64_t)value << (bit - 64u));
    }
}

unsigned integer_equal(integer left, integer right)
{
    return (left.high == right.high) && (left.low == right.low);
}

unsigned integer_less(integer left, integer right)
{
    if (left.high < right.high)
    {
        return 1;
    }
    if (left.high > right.high)
    {
        return 0;
    }
    return (left.low < right.low);
}

integer integer_subtract(integer minuend, integer subtrahend)
{
    integer result = {0, 0};
    result.low = (minuend.low - subtrahend.low);
    if (result.low > minuend.low)
    {
        --minuend.high;
    }
    result.high = (minuend.high - subtrahend.high);
    return result;
}

integer_division integer_divide(integer numerator, integer denominator)
{
    ASSERT(denominator.low || denominator.high);
    integer_division result = {{0, 0}, {0, 0}};
    for (unsigned i = 127; i < 128; --i)
    {
        result.remainder = integer_shift_left(result.remainder, 1);
        result.remainder.low |= integer_bit(numerator, i);
        if (!integer_less(result.remainder, denominator))
        {
            result.remainder = integer_subtract(result.remainder, denominator);
            integer_set_bit(&result.quotient, i, 1);
        }
    }
    return result;
}

char const lower_case_digits[] = "0123456789abcdef";

char *integer_format(integer const value, char const *digits, unsigned base,
                     char *buffer, size_t buffer_size)
{
    ASSUME(base >= 2);
    ASSUME(base <= 16);
    ASSUME(buffer_size > 0);
    size_t next_digit = buffer_size - 1;
    integer rest = value;
    for (;;)
    {
        integer_division const divided =
            integer_divide(rest, integer_create(0, base));
        buffer[next_digit] = digits[divided.remainder.low];
        rest = divided.quotient;
        if (rest.high == 0 && rest.low == 0)
        {
            break;
        }
        if (next_digit == 0)
        {
            return NULL;
        }
        --next_digit;
    }
    return buffer + next_digit;
}