#include "integer.h"
#include "test.h"
#include "lpg_integer.h"

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
}
