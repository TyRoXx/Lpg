#include "lpg_standard_library.h"
#include "lpg_allocate.h"
#include <string.h>
#include "lpg_assert.h"

static void standard_library_stable_free(standard_library_stable *stable)
{
    enumeration_free(&stable->boolean);
    function_pointer_free(&stable->print);
    function_pointer_free(&stable->assert_);
    function_pointer_free(&stable->and_);
    function_pointer_free(&stable->or_);
    function_pointer_free(&stable->not_);
    function_pointer_free(&stable->concat);
    function_pointer_free(&stable->string_equals);
    function_pointer_free(&stable->read);
    function_pointer_free(&stable->int_);
    function_pointer_free(&stable->integer_equals);
    function_pointer_free(&stable->integer_less);
    function_pointer_free(&stable->integer_to_string);
    enumeration_free(&stable->option);
    interface_free(stable->printable);
}

value not_impl(value const *const inferred, value const *arguments, garbage_collector *const gc, void *environment)
{
    (void)environment;
    (void)inferred;
    (void)gc;
    enum_element_id const argument = arguments[0].enum_element.which;
    return value_from_enum_element(!argument, NULL);
}

value concat_impl(value const *const inferred, value const *const arguments, garbage_collector *const gc,
                  void *environment)
{
    (void)inferred;
    (void)environment;
    unicode_view const left = arguments[0].string_ref;
    unicode_view const right = arguments[1].string_ref;
    size_t const result_length = left.length + right.length;
    char *const result = garbage_collector_allocate(gc, result_length);
    memcpy(result, left.begin, left.length);
    memcpy(result + left.length, right.begin, right.length);
    return value_from_string_ref(unicode_view_create(result, result_length));
}

value and_impl(value const *const inferred, value const *arguments, garbage_collector *const gc, void *environment)
{
    (void)environment;
    (void)inferred;
    (void)gc;
    enum_element_id const left = arguments[0].enum_element.which;
    enum_element_id const right = arguments[1].enum_element.which;
    return value_from_enum_element(left && right, NULL);
}

value or_impl(value const *const inferred, value const *arguments, garbage_collector *const gc, void *environment)
{
    (void)environment;
    (void)inferred;
    (void)gc;
    enum_element_id const left = arguments[0].enum_element.which;
    enum_element_id const right = arguments[1].enum_element.which;
    return value_from_enum_element(left || right, NULL);
}

value string_equals_impl(value const *const inferred, value const *const arguments, garbage_collector *const gc,
                         void *environment)
{
    (void)inferred;
    (void)environment;
    (void)gc;
    unicode_view const left = arguments[0].string_ref;
    unicode_view const right = arguments[1].string_ref;
    return value_from_enum_element(unicode_view_equals(left, right), NULL);
}

value int_impl(value const *const inferred, value const *arguments, garbage_collector *const gc, void *environment)
{
    (void)environment;
    (void)inferred;
    (void)gc;
    integer const first = arguments[0].integer_;
    integer const second = arguments[1].integer_;
    if (integer_less(first, second))
    {
        return value_from_type(type_from_integer_range(integer_range_create(first, second)));
    }
    return value_from_type(type_from_integer_range(integer_range_create(second, first)));
}

value integer_equals_impl(value const *const inferred, value const *const arguments, garbage_collector *const gc,
                          void *environment)
{
    (void)inferred;
    (void)environment;
    (void)gc;
    integer const left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    return value_from_enum_element(integer_equal(left, right), NULL);
}

value integer_less_impl(value const *const inferred, value const *const arguments, garbage_collector *const gc,
                        void *environment)
{
    (void)inferred;
    (void)environment;
    (void)gc;
    integer const left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    return value_from_enum_element(integer_less(left, right), NULL);
}

value integer_to_string_impl(value const *const inferred, value const *const arguments, garbage_collector *const gc,
                             void *environment)
{
    (void)inferred;
    (void)environment;
    (void)gc;

    integer const left = arguments[0].integer_;
    const unsigned int printing_base = 10;
    const size_t buffer_max_size = integer_string_max_length(printing_base);

    const size_t bytes = sizeof(char) * buffer_max_size;
    char *buffer = garbage_collector_allocate(gc, bytes);
    char *buffer_begin = integer_format(left, lower_case_digits, printing_base, buffer, buffer_max_size);
    size_t const index_length = (size_t)((buffer + bytes) - buffer_begin);

    return value_from_string_ref(unicode_view_create(buffer_begin, index_length));
}

standard_library_description describe_standard_library(void)
{
    standard_library_stable *const stable = allocate(sizeof(*stable));
    {
        enumeration_element *const boolean_elements = allocate_array(2, sizeof(*boolean_elements));
        boolean_elements[0] = enumeration_element_create(unicode_string_from_c_str("false"), type_from_unit());
        boolean_elements[1] = enumeration_element_create(unicode_string_from_c_str("true"), type_from_unit());
        stable->boolean = enumeration_create(boolean_elements, 2);
    }
    type const boolean = type_from_enumeration(&stable->boolean);

    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(unicode_string_from_c_str("none"), type_from_unit());
        elements[1] = enumeration_element_create(
            unicode_string_from_c_str("some"),
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max())));
        stable->option = enumeration_create(elements, 2);
    }

    stable->print = function_pointer_create(
        type_from_unit(), tuple_type_create(type_allocate(type_from_string_ref()), 1), tuple_type_create(NULL, 0));
    stable->assert_ = function_pointer_create(
        type_from_unit(), tuple_type_create(type_allocate(boolean), 1), tuple_type_create(NULL, 0));
    {
        type *const and_parameters = allocate_array(2, sizeof(*and_parameters));
        and_parameters[0] = boolean;
        and_parameters[1] = boolean;
        stable->and_ =
            function_pointer_create(boolean, tuple_type_create(and_parameters, 2), tuple_type_create(NULL, 0));
    }
    {
        type *const or_parameters = allocate_array(2, sizeof(*or_parameters));
        or_parameters[0] = boolean;
        or_parameters[1] = boolean;
        stable->or_ = function_pointer_create(boolean, tuple_type_create(or_parameters, 2), tuple_type_create(NULL, 0));
    }
    {
        type *const not_parameters = allocate_array(1, sizeof(*not_parameters));
        not_parameters[0] = boolean;
        stable->not_ =
            function_pointer_create(boolean, tuple_type_create(not_parameters, 1), tuple_type_create(NULL, 0));
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_string_ref();
        parameters[1] = type_from_string_ref();
        stable->concat = function_pointer_create(
            type_from_string_ref(), tuple_type_create(parameters, 2), tuple_type_create(NULL, 0));
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_string_ref();
        parameters[1] = type_from_string_ref();
        stable->string_equals =
            function_pointer_create(boolean, tuple_type_create(parameters, 2), tuple_type_create(NULL, 0));
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->int_ =
            function_pointer_create(type_from_type(), tuple_type_create(parameters, 2), tuple_type_create(NULL, 0));
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->integer_equals =
            function_pointer_create(boolean, tuple_type_create(parameters, 2), tuple_type_create(NULL, 0));
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->integer_less =
            function_pointer_create(boolean, tuple_type_create(parameters, 2), tuple_type_create(NULL, 0));
    }
    {
        type *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));

        stable->integer_to_string = function_pointer_create(
            type_from_string_ref(), tuple_type_create(parameters, 1), tuple_type_create(NULL, 0));
    }
    stable->read =
        function_pointer_create(type_from_string_ref(), tuple_type_create(NULL, 0), tuple_type_create(NULL, 0));

    stable->printable = interface_create(NULL, 0);

    structure_member *globals = allocate_array(standard_library_element_count, sizeof(*globals));
    globals[0] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("type"), optional_value_create(value_from_type(type_from_type())));

    globals[1] = structure_member_create(type_from_type(), unicode_string_from_c_str("string-ref"),
                                         optional_value_create(value_from_type(type_from_string_ref())));

    globals[2] = structure_member_create(
        type_from_function_pointer(&stable->print), unicode_string_from_c_str("print"), optional_value_empty);

    globals[3] =
        structure_member_create(type_from_type(), unicode_string_from_c_str("boolean"),
                                optional_value_create(value_from_type(type_from_enumeration(&stable->boolean))));

    globals[4] = structure_member_create(
        type_from_function_pointer(&stable->assert_), unicode_string_from_c_str("assert"), optional_value_empty);

    globals[5] = structure_member_create(
        type_from_function_pointer(&stable->and_), unicode_string_from_c_str("and"),
        optional_value_create(value_from_function_pointer(function_pointer_value_from_external(and_impl, NULL))));

    globals[6] = structure_member_create(
        type_from_function_pointer(&stable->or_), unicode_string_from_c_str("or"),
        optional_value_create(value_from_function_pointer(function_pointer_value_from_external(or_impl, NULL))));

    globals[7] = structure_member_create(
        type_from_function_pointer(&stable->not_), unicode_string_from_c_str("not"),
        optional_value_create(value_from_function_pointer(function_pointer_value_from_external(not_impl, NULL))));

    globals[8] = structure_member_create(
        type_from_function_pointer(&stable->concat), unicode_string_from_c_str("concat"),
        optional_value_create(value_from_function_pointer(function_pointer_value_from_external(concat_impl, NULL))));

    globals[9] = structure_member_create(
        type_from_function_pointer(&stable->string_equals), unicode_string_from_c_str("string-equals"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(string_equals_impl, NULL))));

    globals[10] = structure_member_create(
        type_from_function_pointer(&stable->read), unicode_string_from_c_str("read"), optional_value_empty);

    globals[11] = structure_member_create(
        type_from_function_pointer(&stable->int_), unicode_string_from_c_str("int"),
        optional_value_create(value_from_function_pointer(function_pointer_value_from_external(int_impl, NULL))));

    globals[12] = structure_member_create(
        type_from_function_pointer(&stable->integer_equals), unicode_string_from_c_str("integer-equals"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(integer_equals_impl, NULL))));

    globals[13] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("unit"), optional_value_create(value_from_type(type_from_unit())));

    globals[14] = structure_member_create(
        type_from_unit(), unicode_string_from_c_str("unit_value"), optional_value_create(value_from_unit()));

    globals[15] =
        structure_member_create(type_from_type(), unicode_string_from_c_str("option"),
                                optional_value_create(value_from_type(type_from_enumeration(&stable->option))));

    globals[16] = structure_member_create(
        type_from_function_pointer(&stable->integer_less), unicode_string_from_c_str("integer-less"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(integer_less_impl, NULL))));

    globals[17] = structure_member_create(
        type_from_function_pointer(&stable->integer_to_string), unicode_string_from_c_str("integer-to-string"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(integer_to_string_impl, NULL))));

    globals[18] =
        structure_member_create(type_from_type(), unicode_string_from_c_str("printable"),
                                optional_value_create(value_from_type(type_from_interface(&stable->printable))));

    LPG_STATIC_ASSERT(standard_library_element_count == 19);

    standard_library_description const result = {structure_create(globals, standard_library_element_count), stable};
    return result;
}

void standard_library_description_free(standard_library_description const *value)
{
    structure_free(&value->globals);
    standard_library_stable_free(value->stable);
    deallocate(value->stable);
}
