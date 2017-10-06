#include "lpg_integer.h"

integer integer_create(uint64_t high, uint64_t low)
{
    integer const result = {high, low};
    return result;
}

integer integer_max(void)
{
    return integer_create(~(uint64_t)0, ~(uint64_t)0);
}

integer integer_shift_left_truncate(integer value, uint32_t bits)
{
    if (bits == 0)
    {
        return value;
    }
    if (bits == 64)
    {
        return integer_create(value.low, 0);
    }
    if (bits == 128)
    {
        return integer_create(0, 0);
    }
    if (bits > 64)
    {
        value.high = value.low;
        value.low = 0;
        bits -= 64;
    }
    integer const result = {
        ((value.high << bits) | ((value.low >> (64u - bits)) & ((uint64_t)-1) >> (64u - bits))), (value.low << bits)};
    return result;
}

bool integer_shift_left(integer *value, uint32_t bits)
{
    if (bits >= 64)
    {
        if (value->high != 0)
        {
            return 0;
        }
        if ((bits > 64) && value->low & (UINT64_MAX << (128u - bits)))
        {
            return 0;
        }
    }
    else if ((bits > 0) && (value->high & (UINT64_MAX << (64u - bits))))
    {
        return 0;
    }
    *value = integer_shift_left_truncate(*value, bits);
    return 1;
}

bool integer_bit(integer value, uint32_t bit)
{
    if (bit < 64u)
    {
        return (value.low >> bit) & 1u;
    }
    return (value.high >> (bit - 64u)) & 1u;
}

void integer_set_bit(integer *target, uint32_t bit, bool value)
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

bool integer_equal(integer left, integer right)
{
    return (left.high == right.high) && (left.low == right.low);
}

bool integer_less(integer left, integer right)
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

bool integer_less_or_equals(integer left, integer right)
{
    return !integer_less(right, left);
}

integer integer_subtract(integer minuend, integer subtrahend)
{
    integer result
#ifndef _MSC_VER
        =
    {
        0,
        0
    }
#endif
    ;
    result.low = (minuend.low - subtrahend.low);
    if (result.low > minuend.low)
    {
        --minuend.high;
    }
    result.high = (minuend.high - subtrahend.high);
    return result;
}

bool integer_multiply(integer *left, integer right)
{
    integer product = integer_create(0, 0);
    for (uint32_t i = 0; i < 128; ++i)
    {
        if (integer_bit(right, i))
        {
            integer summand = *left;
            if (!integer_shift_left(&summand, i))
            {
                return 0;
            }
            if (!integer_add(&product, summand))
            {
                return 0;
            }
        }
    }
    *left = product;
    return 1;
}

bool integer_add(integer *left, integer right)
{
    uint64_t const low = (left->low + right.low);
    uint64_t high = left->high;
    if (low < right.low)
    {
        if (high == UINT64_MAX)
        {
            return 0;
        }
        ++high;
    }
    high += right.high;
    if (high >= right.high)
    {
        left->high = high;
        left->low = low;
        return 1;
    }
    return 0;
}

bool integer_parse(integer *into, unicode_view from)
{
    ASSUME(into);
    ASSUME(from.length >= 1);
    integer result = integer_create(0, 0);
    for (size_t i = 0; i < from.length; ++i)
    {
        if (!integer_multiply(&result, integer_create(0, 10)))
        {
            return 0;
        }
        if (!integer_add(&result, integer_create(0, (uint64_t)(from.begin[i] - '0'))))
        {
            return 0;
        }
    }
    *into = result;
    return 1;
}

integer_division integer_divide(integer numerator, integer denominator)
{
    ASSERT(denominator.low || denominator.high);
    integer_division result = {{0, 0}, {0, 0}};
    for (unsigned i = 0; i < 128u; ++i)
    {
        result.remainder = integer_shift_left_truncate(result.remainder, 1);
        result.remainder.low |= integer_bit(numerator, 127u - i);
        if (!integer_less(result.remainder, denominator))
        {
            result.remainder = integer_subtract(result.remainder, denominator);
            integer_set_bit(&result.quotient, 127u - i, 1);
        }
    }
    return result;
}

char const lower_case_digits[] = "0123456789abcdef";

char *integer_format(integer const value, char const *digits, unsigned base, char *buffer, size_t buffer_size)
{
    ASSUME(base >= 2);
    ASSUME(base <= 16);
    ASSUME(buffer_size > 0);
    size_t next_digit = buffer_size - 1;
    integer rest = value;
    for (;;)
    {
        integer_division const divided = integer_divide(rest, integer_create(0, base));
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

size_t integer_string_max_length(unsigned int base)
{
    ASSUME(base >= 2);
    unsigned int length_base2 = sizeof(integer) * 8;
    return (size_t)ceil(length_base2 / log2(base));
}