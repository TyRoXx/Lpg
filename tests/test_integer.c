#include "test_integer.h"
#include "lpg_integer.h"
#include "test.h"
#include <string.h>

static void test_add_success(integer left, integer right, integer expected)
{
    REQUIRE(integer_add(&left, right));
    REQUIRE(integer_equal(left, expected));
}

static void test_add_overflow(integer left, integer right)
{
    integer result = left;
    REQUIRE(!integer_add(&result, right));
    REQUIRE(integer_equal(result, left));
}

static void test_multiply_success(integer left, integer right, integer expected)
{
    REQUIRE(integer_multiply(&left, right));
    REQUIRE(integer_equal(left, expected));
}

static void test_multiply_overflow(integer left, integer right)
{
    integer result = left;
    REQUIRE(!integer_multiply(&result, right));
    REQUIRE(integer_equal(result, left));
}

static void test_shift_left_success(integer left, uint32_t bits, integer expected)
{
    integer result = left;
    REQUIRE(integer_shift_left(&result, bits));
    REQUIRE(integer_equal(result, expected));
}

static void test_shift_left_overflow(integer left, uint32_t bits)
{
    integer result = left;
    REQUIRE(!integer_shift_left(&result, bits));
    REQUIRE(integer_equal(result, left));
}

static void test_parse_integer_success(integer expected, char const *input)
{
    integer parsed;
    unicode_string input_string = unicode_string_from_c_str(input);
    REQUIRE(integer_parse(&parsed, unicode_view_from_string(input_string)));
    REQUIRE(integer_equal(expected, parsed));
    unicode_string_free(&input_string);
}

static void test_parse_integer_overflow(char const *input)
{
    integer parsed;
    unicode_string input_string = unicode_string_from_c_str(input);
    REQUIRE(!integer_parse(&parsed, unicode_view_from_string(input_string)));
    unicode_string_free(&input_string);
}

static void test_add(void);
static void test_multiply(void);
static void test_shift_left(void);
static void test_parse_integer(void);
static void test_integer_divide(void);
static void test_integer_equals(void);
static void test_integer_less(void);
static void test_integer_format(void);
static void test_integer_maximum_and_minimum(void);

void test_integer(void)
{
    test_integer_less();
    test_integer_equals();

    REQUIRE(integer_equal(integer_shift_left_truncate(integer_create(0, 1), 0), integer_create(0, 1)));
    REQUIRE(integer_equal(integer_shift_left_truncate(integer_create(0, 1), 1), integer_create(0, 2)));
    REQUIRE(integer_equal(integer_shift_left_truncate(integer_create(0, 1), 64), integer_create(1, 0)));

    REQUIRE(integer_difference_equals(
        integer_difference_create(integer_create(0, 0)), integer_subtract(integer_create(0, 0), integer_create(0, 0))));
    REQUIRE(integer_difference_equals(
        integer_difference_create(integer_create(0, 1)), integer_subtract(integer_create(0, 1), integer_create(0, 0))));
    REQUIRE(integer_difference_equals(
        integer_difference_create(integer_create(0, 0)), integer_subtract(integer_create(1, 0), integer_create(1, 0))));
    REQUIRE(integer_difference_equals(
        integer_difference_negative(), integer_subtract(integer_create(0, 0), integer_create(0, 1))));
    REQUIRE(integer_difference_equals(
        integer_difference_negative(), integer_subtract(integer_create(0, 0), integer_create(0, 2))));

    test_integer_divide();

    test_integer_format();

    test_add();
    test_multiply();
    test_shift_left();
    test_parse_integer();
    test_integer_maximum_and_minimum();
}

static void test_integer_maximum_and_minimum(void)
{
    integer low = integer_create(0, 10);
    integer high = integer_create(0, 20);

    REQUIRE(integer_equal(integer_maximum(low, high), high));
    REQUIRE(integer_equal(integer_minimum(low, high), low));
}

static void test_integer_format(void)
{
    for (unsigned base = 2; base <= 16; ++base)
    {
        for (unsigned value = 2; value < base; ++value)
        {
            char buffer[1];
            unicode_view const formatted =
                integer_format(integer_create(0, value), lower_case_digits, base, buffer, sizeof(buffer));
            REQUIRE(formatted.begin == buffer);
            REQUIRE(buffer[0] == lower_case_digits[value]);
        }
    }
    {
        char buffer[39];
        unicode_view const formatted = integer_format(
            integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu), lower_case_digits, 10, buffer, sizeof(buffer));
        REQUIRE(buffer == formatted.begin);
        REQUIRE(!memcmp(buffer, "340282366920938463463374607431768211455", sizeof(buffer)));
    }
    {
        char buffer[32];
        unicode_view const formatted = integer_format(
            integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu), lower_case_digits, 16, buffer, sizeof(buffer));
        REQUIRE(buffer == formatted.begin);
        REQUIRE(!memcmp(buffer, "ffffffffffffffffffffffffffffffff", sizeof(buffer)));
    }
    {
        char buffer[38];
        unicode_view const formatted = integer_format(
            integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu), lower_case_digits, 10, buffer, sizeof(buffer));
        REQUIRE(NULL == formatted.begin);
        REQUIRE(0 == formatted.length);
    }
}

static void test_integer_less(void)
{
    REQUIRE(integer_less(integer_create(0, 0), integer_create(0, 1)));
    REQUIRE(integer_less(integer_create(0, 0), integer_create(1, 1)));
    REQUIRE(integer_less(integer_create(0, 0), integer_create(111, 111)));
    REQUIRE(integer_less(integer_create(0, 2), integer_create(0, 4)));
    REQUIRE(integer_less(integer_create(0, 2), integer_create(1, 0)));
    REQUIRE(!integer_less(integer_create(0, 2), integer_create(0, 1)));
    REQUIRE(!integer_less(integer_create(1, 2), integer_create(1, 1)));
    REQUIRE(!integer_less(integer_create(111, 112), integer_create(111, 111)));
    REQUIRE(!integer_less(integer_create(0, 5), integer_create(0, 4)));
    REQUIRE(!integer_less(integer_create(2, 0), integer_create(1, 0)));
}

static void test_integer_equals(void)
{
    REQUIRE(integer_equal(integer_create(0, 0), integer_create(0, 0)));
    REQUIRE(integer_equal(integer_create(1, 1), integer_create(1, 1)));
    REQUIRE(!integer_equal(integer_create(1, 0), integer_create(0, 0)));
    REQUIRE(!integer_equal(integer_create(0, 1), integer_create(0, 0)));
    REQUIRE(!integer_equal(integer_create(0, 0), integer_create(1, 0)));
    REQUIRE(!integer_equal(integer_create(0, 0), integer_create(0, 1)));
}

static void test_integer_divide(void)
{
    {
        integer_division result = integer_divide(integer_create(0, 0), integer_create(0, 1));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 0)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 0)));
    }
    {
        integer_division result = integer_divide(integer_create(0, 1), integer_create(0, 1));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 1)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 0)));
    }
    {
        integer_division result = integer_divide(integer_create(0, 10), integer_create(0, 10));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 1)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 0)));
    }
    {
        integer_division result = integer_divide(integer_create(0, 10), integer_create(0, 3));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 3)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 1)));
    }
    {
        integer_division result = integer_divide(integer_create(2, 13), integer_create(0, 2));
        REQUIRE(integer_equal(result.quotient, integer_create(1, 6)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 1)));
    }
    {
        integer_division result = integer_divide(integer_create(1, 0), integer_create(0, 10));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 1844674407370955161)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 6)));
    }
}

static void test_parse_integer(void)
{
    test_parse_integer_success(integer_create(0, 0), "0");
    test_parse_integer_success(integer_create(0, 1), "1");
    test_parse_integer_success(integer_create(0, 10), "10");
    test_parse_integer_success(integer_create(0, 42), "42");
    test_parse_integer_success(
        integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu), "340282366920938463463374607431768211455");
    test_parse_integer_overflow("340282366920938463463374607431768211456");
    test_parse_integer_overflow("340282366920938463463374607431768211457");
    test_parse_integer_overflow("340282366920938463463374607431768211458");
    test_parse_integer_overflow("340282366920938463463374607431768211459");
    test_parse_integer_overflow("340282366920938463463374607431768211460");
    test_parse_integer_overflow("340282366920938463463374607431768211461");
    test_parse_integer_overflow("340282366920938463463374607431768211462");
    test_parse_integer_overflow("340282366920938463463374607431768211463");
    test_parse_integer_overflow("340282366920938463463374607431768211464");
    test_parse_integer_overflow("340282366920938463463374607431768211465");
}

static void test_shift_left(void)
{
    for (uint32_t i = 0; i <= 128; ++i)
    {
        test_shift_left_success(integer_create(0, 0), i, integer_create(0, 0));
    }
    test_shift_left_success(integer_create(0, 1), 0, integer_create(0, 1));
    test_shift_left_success(integer_create(0, 1), 1, integer_create(0, 2));
    test_shift_left_success(integer_create(0, 1), 2, integer_create(0, 4));
    test_shift_left_success(integer_create(0, 1), 3, integer_create(0, 8));
    test_shift_left_success(integer_create(0, 1), 4, integer_create(0, 16));
    test_shift_left_success(integer_create(0, 1), 64, integer_create(1, 0));
    test_shift_left_success(integer_create(0, 3), 63, integer_create(1, 0x8000000000000000u));
    test_shift_left_success(integer_create(0, 1), 127, integer_create(0x8000000000000000u, 0));
    test_shift_left_overflow(integer_create(0, 1), 128);
    test_shift_left_overflow(integer_create(0, 2), 127);
    test_shift_left_overflow(integer_create(1, 0), 64);
    test_shift_left_overflow(integer_create(1, 0), 65);
    test_shift_left_overflow(integer_create(2, 0), 63);
}

static void test_multiply(void)
{
    test_multiply_success(integer_create(0, 0), integer_create(0, 0), integer_create(0, 0));
    test_multiply_success(integer_create(0, 1), integer_create(0, 0), integer_create(0, 0));
    test_multiply_success(integer_create(0, 0), integer_create(0, 1), integer_create(0, 0));
    test_multiply_success(integer_create(0, 1), integer_create(0, 1), integer_create(0, 1));
    test_multiply_success(integer_create(0, 3), integer_create(0, 4), integer_create(0, 12));
    test_multiply_success(
        integer_create(0, 2), integer_create(0, 0xFFFFFFFFFFFFFFFFu), integer_create(1, 0xFFFFFFFFFFFFFFFEu));
    test_multiply_success(integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu), integer_create(0, 1),
                          integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu));
    test_multiply_overflow(integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu),
                           integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu));
    test_multiply_overflow(integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu), integer_create(0, 2));
}

static void test_add(void)
{
    test_add_success(integer_create(0, 0), integer_create(0, 0), integer_create(0, 0));
    test_add_success(integer_create(0, 1), integer_create(0, 0), integer_create(0, 1));
    test_add_success(integer_create(0, 0), integer_create(0, 1), integer_create(0, 1));
    test_add_success(integer_create(0, 1), integer_create(0, 1), integer_create(0, 2));
    test_add_success(integer_create(1, 0), integer_create(0, 0), integer_create(1, 0));
    test_add_success(integer_create(0, 0), integer_create(1, 0), integer_create(1, 0));
    test_add_success(integer_create(1, 0), integer_create(1, 0), integer_create(2, 0));
    test_add_success(integer_create(0, 0xFFFFFFFFFFFFFFFFu), integer_create(0, 0xFFFFFFFFFFFFFFFFu),
                     integer_create(1, 0xFFFFFFFFFFFFFFFEu));
    test_add_overflow(integer_create(0xFFFFFFFFFFFFFFFFu, 0), integer_create(0xFFFFFFFFFFFFFFFFu, 0));
    test_add_overflow(integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu),
                      integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu));
}
