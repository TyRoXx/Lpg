#include "test_implicitly_convertible.h"
#include "lpg_check.h"
#include "test.h"

void test_integer_convertible()
{
    type small_range = type_from_integer_range(
        integer_range_create(integer_create(20, 10), integer_create(30, 7)));
    type big_range = type_from_integer_range(
        integer_range_create(integer_create(0, 0), integer_create(700, 0)));
    REQUIRE(is_implicitly_convertible(small_range, big_range));
    REQUIRE(!is_implicitly_convertible(big_range, small_range));
}

void test_function_pointer_convertible()
{
    type small_range = type_from_integer_range(
        integer_range_create(integer_create(20, 10), integer_create(30, 7)));
    type big_range = type_from_integer_range(
        integer_range_create(integer_create(0, 0), integer_create(700, 0)));
    type arguments[] = {small_range, small_range};
    function_pointer function_pointer1 =
        function_pointer_create(small_range, arguments, 2);
    type function1 = type_from_function_pointer(&function_pointer1);

    REQUIRE(is_implicitly_convertible(function1, function1));
    {
        function_pointer function_pointer2 =
            function_pointer_create(big_range, arguments, 2);
        type function2 = type_from_function_pointer(&function_pointer2);

        REQUIRE(!is_implicitly_convertible(function1, function2));
    }
    {
        type more_arguments[] = {small_range, small_range, small_range};
        function_pointer function_pointer2 =
            function_pointer_create(big_range, more_arguments, 3);
        type function2 = type_from_function_pointer(&function_pointer2);

        REQUIRE(!is_implicitly_convertible(function1, function2));
    }
    {
        type different_arguments[] = {small_range, big_range};
        function_pointer function_pointer2 =
            function_pointer_create(big_range, different_arguments, 2);
        type function2 = type_from_function_pointer(&function_pointer2);

        REQUIRE(!is_implicitly_convertible(function1, function2));
    }
}

void test_implicitly_convertible(void)
{
    test_integer_convertible();
    test_function_pointer_convertible();
}
