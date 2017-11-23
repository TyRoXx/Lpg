#include "test_value.h"
#include "test.h"
#include "lpg_value.h"

static void test_integer_comparison(void);

void test_value(void)
{
    REQUIRE(!value_equals(value_from_unit(), value_from_string_ref(unicode_view_from_c_str(""))));
    {
        value element = value_from_unit();
        REQUIRE(!value_equals(
            value_from_tuple(value_tuple_create(NULL, 0)), value_from_tuple(value_tuple_create(&element, 1))));
    }
    {
        value first = value_from_unit();
        value second = value_from_unit();
        REQUIRE(value_equals(
            value_from_tuple(value_tuple_create(&first, 1)), value_from_tuple(value_tuple_create(&second, 1))));
    }
    {
        value first = value_from_unit();
        value second = value_from_string_ref(unicode_view_from_c_str(""));
        REQUIRE(!value_equals(
            value_from_tuple(value_tuple_create(&first, 1)), value_from_tuple(value_tuple_create(&second, 1))));
    }
    REQUIRE(!value_equals(
        value_from_enum_element(0, type_from_unit(), NULL), value_from_enum_element(1, type_from_unit(), NULL)));
    REQUIRE(!value_equals(
        value_from_enum_element(0, type_from_unit(), NULL),
        value_from_enum_element(0, type_from_integer_range(integer_range_create(integer_max(), integer_max())), NULL)));

    test_integer_comparison();
}

static void test_integer_comparison(void)
{
    value small = value_from_integer(integer_create(10, 200));
    value small2 = value_from_integer(integer_create(10, 200));
    value big = value_from_integer(integer_create(302, 970));

    REQUIRE(value_less_than(small, big));
    REQUIRE(value_greater_than(big, small));

    REQUIRE(value_equals(small, small2));
    REQUIRE(!value_less_than(small, small2));
    REQUIRE(!value_greater_than(small, small2));
}