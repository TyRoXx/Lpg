#include "test_value.h"
#include "lpg_array_size.h"
#include "lpg_standard_library.h"
#include "lpg_value.h"
#include "test.h"

static void test_integer_comparison(void);

void test_value(void)
{
    REQUIRE(!value_equals(value_from_unit(), value_from_string(unicode_view_from_c_str(""))));
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
        value second = value_from_string(unicode_view_from_c_str(""));
        REQUIRE(!value_equals(
            value_from_tuple(value_tuple_create(&first, 1)), value_from_tuple(value_tuple_create(&second, 1))));
    }
    REQUIRE(!value_equals(
        value_from_enum_element(0, type_from_unit(), NULL), value_from_enum_element(1, type_from_unit(), NULL)));
    REQUIRE(!value_equals(
        value_from_enum_element(0, type_from_unit(), NULL),
        value_from_enum_element(0, type_from_integer_range(integer_range_create(integer_max(), integer_max())), NULL)));

    REQUIRE(implementation_ref_equals(implementation_ref_create(0, 1), implementation_ref_create(0, 1)));
    REQUIRE(!implementation_ref_equals(implementation_ref_create(0, 1), implementation_ref_create(0, 2)));
    REQUIRE(!implementation_ref_equals(implementation_ref_create(0, 1), implementation_ref_create(1, 1)));

    REQUIRE(!function_pointer_value_equals(
        function_pointer_value_from_internal(0, NULL, 0), function_pointer_value_from_internal(1, NULL, 0)));
    {
        value captures[1] = {value_from_unit()};
        REQUIRE(!function_pointer_value_equals(
            function_pointer_value_from_internal(0, NULL, 0),
            function_pointer_value_from_internal(0, captures, LPG_ARRAY_SIZE(captures))));
    }
    {
        value left_captures[2] = {value_from_unit(), value_from_unit()};
        value right_captures[2] = {value_from_unit(), value_from_integer(integer_create(0, 0))};
        REQUIRE(!function_pointer_value_equals(
            function_pointer_value_from_internal(0, left_captures, LPG_ARRAY_SIZE(left_captures)),
            function_pointer_value_from_internal(0, right_captures, LPG_ARRAY_SIZE(right_captures))));
    }
    REQUIRE(!function_pointer_value_equals(
        function_pointer_value_from_internal(0, NULL, 0),
        function_pointer_value_from_external(
            integer_equals_impl, NULL, NULL,
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty()))));
    REQUIRE(!function_pointer_value_equals(
        function_pointer_value_from_external(
            integer_equals_impl, NULL, NULL,
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty())),
        function_pointer_value_from_internal(0, NULL, 0)));
    REQUIRE(!function_pointer_value_equals(
        function_pointer_value_from_external(
            integer_equals_impl, NULL, NULL,
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty())),
        function_pointer_value_from_external(
            integer_less_impl, NULL, NULL,
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty()))));
    REQUIRE(value_equals(value_from_type(type_from_unit()), value_from_type(type_from_unit())));
    REQUIRE(!value_less_than(value_from_type(type_from_unit()), value_from_integer(integer_create(0, 0))));
    REQUIRE(!value_less_than(value_from_integer(integer_create(0, 0)), value_from_integer(integer_create(0, 0))));
    REQUIRE(value_less_than(
        value_from_string(unicode_view_from_c_str("")), value_from_string(unicode_view_from_c_str("1"))));
    REQUIRE(value_less_than(
        value_from_enum_element(0, type_from_unit(), NULL), value_from_enum_element(1, type_from_unit(), NULL)));
    REQUIRE(!value_less_than(
        value_from_enum_element(0, type_from_unit(), NULL), value_from_enum_element(0, type_from_unit(), NULL)));

    REQUIRE(array_value_equals(
        array_value_create(NULL, 0, 0, type_from_unit()), array_value_create(NULL, 0, 0, type_from_unit())));
    REQUIRE(!array_value_equals(
        array_value_create(NULL, 0, 0, type_from_unit()), array_value_create(NULL, 0, 0, type_from_generic_enum())));
    {
        value element = value_from_unit();
        REQUIRE(!array_value_equals(
            array_value_create(&element, 1, 0, type_from_unit()), array_value_create(NULL, 0, 0, type_from_unit())));
    }
    {
        value left = value_from_integer(integer_create(0, 0));
        value right = value_from_integer(integer_create(0, 1));
        REQUIRE(!array_value_equals(
            array_value_create(&left, 1, 1, type_from_unit()), array_value_create(&right, 1, 1, type_from_unit())));
    }
    {
        value left = value_from_integer(integer_create(0, 0));
        value right = value_from_integer(integer_create(0, 0));
        REQUIRE(!array_value_equals(
            array_value_create(&left, 1, 1, type_from_unit()), array_value_create(&right, 1, 2, type_from_unit())));
    }
    REQUIRE(
        value_equals(value_from_string(unicode_view_from_c_str("")), value_from_string(unicode_view_from_c_str(""))));

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
