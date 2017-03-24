#include "integer.h"
#include "test.h"
#include "lpg_integer.h"
#include <string.h>

void test_integer(void)
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

    REQUIRE(integer_equal(integer_create(0, 0), integer_create(0, 0)));
    REQUIRE(integer_equal(integer_create(1, 1), integer_create(1, 1)));
    REQUIRE(!integer_equal(integer_create(1, 0), integer_create(0, 0)));
    REQUIRE(!integer_equal(integer_create(0, 1), integer_create(0, 0)));
    REQUIRE(!integer_equal(integer_create(0, 0), integer_create(1, 0)));
    REQUIRE(!integer_equal(integer_create(0, 0), integer_create(0, 1)));

    REQUIRE(integer_equal(
        integer_shift_left(integer_create(0, 1), 0), integer_create(0, 1)));
    REQUIRE(integer_equal(
        integer_shift_left(integer_create(0, 1), 1), integer_create(0, 2)));
    REQUIRE(integer_equal(
        integer_shift_left(integer_create(0, 1), 64), integer_create(1, 0)));

    REQUIRE(integer_equal(
        integer_create(0, 0),
        integer_subtract(integer_create(0, 0), integer_create(0, 0))));
    REQUIRE(integer_equal(
        integer_create(0, 1),
        integer_subtract(integer_create(0, 1), integer_create(0, 0))));
    REQUIRE(integer_equal(
        integer_create(0, 0),
        integer_subtract(integer_create(1, 0), integer_create(1, 0))));
    REQUIRE(integer_equal(
        integer_create(~(uint64_t)0, ~(uint64_t)0),
        integer_subtract(integer_create(0, 0), integer_create(0, 1))));
    REQUIRE(integer_equal(
        integer_create(~(uint64_t)0, ~(uint64_t)0 - 1u),
        integer_subtract(integer_create(0, 0), integer_create(0, 2))));

    {
        integer_division result =
            integer_divide(integer_create(0, 0), integer_create(0, 1));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 0)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 0)));
    }
    {
        integer_division result =
            integer_divide(integer_create(0, 1), integer_create(0, 1));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 1)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 0)));
    }
    {
        integer_division result =
            integer_divide(integer_create(0, 10), integer_create(0, 10));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 1)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 0)));
    }
    {
        integer_division result =
            integer_divide(integer_create(0, 10), integer_create(0, 3));
        REQUIRE(integer_equal(result.quotient, integer_create(0, 3)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 1)));
    }
    {
        integer_division result =
            integer_divide(integer_create(2, 13), integer_create(0, 2));
        REQUIRE(integer_equal(result.quotient, integer_create(1, 6)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 1)));
    }
    {
        integer_division result =
            integer_divide(integer_create(1, 0), integer_create(0, 10));
        REQUIRE(integer_equal(
            result.quotient, integer_create(0, 1844674407370955161)));
        REQUIRE(integer_equal(result.remainder, integer_create(0, 6)));
    }
    for (unsigned base = 2; base <= 16; ++base)
    {
        for (unsigned value = 2; value < base; ++value)
        {
            char buffer[1];
            char *formatted =
                integer_format(integer_create(0, value), lower_case_digits,
                               base, buffer, sizeof(buffer));
            REQUIRE(formatted == buffer);
            REQUIRE(buffer[0] == lower_case_digits[value]);
        }
    }
    {
        char buffer[39];
        char *formatted = integer_format(
            integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu),
            lower_case_digits, 10, buffer, sizeof(buffer));
        REQUIRE(buffer == formatted);
        REQUIRE(!memcmp(
            buffer, "340282366920938463463374607431768211455", sizeof(buffer)));
    }
    {
        char buffer[32];
        char *formatted = integer_format(
            integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu),
            lower_case_digits, 16, buffer, sizeof(buffer));
        REQUIRE(buffer == formatted);
        REQUIRE(!memcmp(
            buffer, "ffffffffffffffffffffffffffffffff", sizeof(buffer)));
    }
    {
        char buffer[38];
        REQUIRE(NULL ==
                integer_format(
                    integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu),
                    lower_case_digits, 10, buffer, sizeof(buffer)));
    }
}
