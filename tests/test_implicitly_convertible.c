#include "test_implicitly_convertible.h"
#include "lpg_check.h"
#include "test.h"

static void test_integer_convertible(void)
{
    type small_range = type_from_integer_range(
        integer_range_create(integer_create(20, 10), integer_create(30, 7)));
    type big_range = type_from_integer_range(
        integer_range_create(integer_create(0, 0), integer_create(700, 0)));
    REQUIRE(is_implicitly_convertible(small_range, big_range));
    REQUIRE(!is_implicitly_convertible(big_range, small_range));
}

static void test_function_pointer_convertible(void)
{
    type small_range = type_from_integer_range(
        integer_range_create(integer_create(20, 10), integer_create(30, 7)));
    type big_range = type_from_integer_range(
        integer_range_create(integer_create(0, 0), integer_create(700, 0)));
    type arguments[] = {small_range, small_range};
    function_pointer function_pointer1 =
        function_pointer_create(small_range, tuple_type_create(arguments, 2));
    type function1 = type_from_function_pointer(&function_pointer1);

    REQUIRE(is_implicitly_convertible(function1, function1));
    {
        function_pointer function_pointer2 =
            function_pointer_create(big_range, tuple_type_create(arguments, 2));
        type function2 = type_from_function_pointer(&function_pointer2);

        REQUIRE(!is_implicitly_convertible(function1, function2));
    }
    {
        type more_arguments[] = {small_range, small_range, small_range};
        function_pointer function_pointer2 = function_pointer_create(
            big_range, tuple_type_create(more_arguments, 3));
        type function2 = type_from_function_pointer(&function_pointer2);

        REQUIRE(!is_implicitly_convertible(function1, function2));
    }
    {
        type different_arguments[] = {small_range, big_range};
        function_pointer function_pointer2 = function_pointer_create(
            big_range, tuple_type_create(different_arguments, 2));
        type function2 = type_from_function_pointer(&function_pointer2);

        REQUIRE(!is_implicitly_convertible(function1, function2));
    }
}

static void test_tuple_implicitly_convertible(void)
{
    {
        // Checks if a tuple is implicitly castable to itself.
        type type_list = type_from_integer_range(integer_range_create(
            integer_create(10, 100), integer_create(100, 30)));
        tuple_type tuple_type1 = {&type_list, 1};

        type left = type_from_tuple_type(tuple_type1);
        type right = type_from_tuple_type(tuple_type1);
        REQUIRE(is_implicitly_convertible(left, right));
    }
    {
        type big_int_type = type_from_integer_range(integer_range_create(
            integer_create(10, 100), integer_create(100, 30)));
        type small_int_type = type_from_integer_range(integer_range_create(
            integer_create(1, 100), integer_create(200, 70)));
        tuple_type left_tuple_type = {&big_int_type, 1};
        tuple_type right_tuple_type = {&small_int_type, 1};

        type left = type_from_tuple_type(left_tuple_type);
        type right = type_from_tuple_type(right_tuple_type);
        REQUIRE(is_implicitly_convertible(left, right));
    }
}

void test_implicitly_convertible(void)
{
    test_integer_convertible();
    test_function_pointer_convertible();
    test_tuple_implicitly_convertible();
}
