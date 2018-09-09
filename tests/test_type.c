#include "test_type.h"
#include "test.h"
#include "lpg_type.h"

void test_type(void)
{
    {
        function_pointer const left =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        function_pointer const right = function_pointer_create(
            optional_type_create_set(
                type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, 0)))),
            tuple_type_create(NULL, 0), tuple_type_create(NULL, 0), optional_type_create_empty());
        REQUIRE(!type_equals(type_from_function_pointer(&left), type_from_function_pointer(&right)));
    }
    {
        function_pointer const left =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        type arguments = type_from_unit();
        function_pointer const right =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(&arguments, 1),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        REQUIRE(!type_equals(type_from_function_pointer(&left), type_from_function_pointer(&right)));
    }
    {
        type left_arguments = type_from_unit();
        function_pointer const left =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(&left_arguments, 1),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        type right_arguments =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, 0)));
        function_pointer const right =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(&right_arguments, 1),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        REQUIRE(!type_equals(type_from_function_pointer(&left), type_from_function_pointer(&right)));
    }
}
