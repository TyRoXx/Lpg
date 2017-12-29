#include "lpg_standard_library.h"
#include "lpg_allocate.h"
#include <string.h>
#include "lpg_assert.h"

static void standard_library_stable_free(standard_library_stable *stable)
{
    function_pointer_free(&stable->assert_);
    function_pointer_free(&stable->and_);
    function_pointer_free(&stable->or_);
    function_pointer_free(&stable->not_);
    function_pointer_free(&stable->concat);
    function_pointer_free(&stable->string_equals);
    function_pointer_free(&stable->int_);
    function_pointer_free(&stable->integer_equals);
    function_pointer_free(&stable->integer_less);
    function_pointer_free(&stable->integer_to_string);
    function_pointer_free(&stable->side_effect);
}

value not_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)environment;
    (void)captures;
    enum_element_id const argument = arguments.arguments[0].enum_element.which;
    return value_from_enum_element(!argument, get_boolean(&arguments), NULL);
}

value concat_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)environment;
    (void)captures;
    unicode_view const left = arguments.arguments[0].string_ref;
    unicode_view const right = arguments.arguments[1].string_ref;
    size_t const result_length = left.length + right.length;
    char *const result = garbage_collector_allocate(arguments.gc, result_length);
    memcpy(result, left.begin, left.length);
    memcpy(result + left.length, right.begin, right.length);
    return value_from_string_ref(unicode_view_create(result, result_length));
}

value and_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)environment;
    (void)captures;
    enum_element_id const left = arguments.arguments[0].enum_element.which;
    enum_element_id const right = arguments.arguments[1].enum_element.which;
    return value_from_enum_element(left && right, get_boolean(&arguments), NULL);
}

value or_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)environment;
    (void)captures;
    enum_element_id const left = arguments.arguments[0].enum_element.which;
    enum_element_id const right = arguments.arguments[1].enum_element.which;
    return value_from_enum_element(left || right, get_boolean(&arguments), NULL);
}

value string_equals_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)environment;
    (void)captures;
    unicode_view const left = arguments.arguments[0].string_ref;
    unicode_view const right = arguments.arguments[1].string_ref;
    return value_from_enum_element(unicode_view_equals(left, right), get_boolean(&arguments), NULL);
}

value int_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)environment;
    (void)captures;
    integer const first = arguments.arguments[0].integer_;
    integer const second = arguments.arguments[1].integer_;
    if (integer_less(first, second))
    {
        return value_from_type(type_from_integer_range(integer_range_create(first, second)));
    }
    return value_from_type(type_from_integer_range(integer_range_create(second, first)));
}

value integer_equals_impl(function_call_arguments const arguments, struct value const *const captures,
                          void *environment)
{
    (void)environment;
    (void)captures;
    integer const left = arguments.arguments[0].integer_;
    integer const right = arguments.arguments[1].integer_;
    return value_from_enum_element(integer_equal(left, right), get_boolean(&arguments), NULL);
}

value integer_less_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)environment;
    (void)captures;
    integer const left = arguments.arguments[0].integer_;
    integer const right = arguments.arguments[1].integer_;
    return value_from_enum_element(integer_less(left, right), get_boolean(&arguments), NULL);
}

value integer_to_string_impl(function_call_arguments const arguments, struct value const *const captures,
                             void *environment)
{
    (void)environment;
    (void)captures;

    integer const left = arguments.arguments[0].integer_;
    const unsigned int printing_base = 10;
    const size_t buffer_max_size = 20;
    char *buffer = garbage_collector_allocate(arguments.gc, buffer_max_size);
    char *buffer_begin = integer_format(left, lower_case_digits, printing_base, buffer, buffer_max_size);
    size_t const index_length = (size_t)((buffer + buffer_max_size) - buffer_begin);

    return value_from_string_ref(unicode_view_create(buffer_begin, index_length));
}

value side_effect_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)arguments;
    (void)captures;
    (void)environment;
    return value_from_unit();
}

standard_library_description describe_standard_library(void)
{
    standard_library_stable *const stable = allocate(sizeof(*stable));

    type const boolean = type_from_enumeration(0);

    stable->assert_ = function_pointer_create(type_from_unit(), tuple_type_create(type_allocate(boolean), 1),
                                              tuple_type_create(NULL, 0), optional_type_create_empty());
    {
        type *const and_parameters = allocate_array(2, sizeof(*and_parameters));
        and_parameters[0] = boolean;
        and_parameters[1] = boolean;
        stable->and_ = function_pointer_create(
            boolean, tuple_type_create(and_parameters, 2), tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const or_parameters = allocate_array(2, sizeof(*or_parameters));
        or_parameters[0] = boolean;
        or_parameters[1] = boolean;
        stable->or_ = function_pointer_create(
            boolean, tuple_type_create(or_parameters, 2), tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const not_parameters = allocate_array(1, sizeof(*not_parameters));
        not_parameters[0] = boolean;
        stable->not_ = function_pointer_create(
            boolean, tuple_type_create(not_parameters, 1), tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_string_ref();
        parameters[1] = type_from_string_ref();
        stable->concat = function_pointer_create(type_from_string_ref(), tuple_type_create(parameters, 2),
                                                 tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_string_ref();
        parameters[1] = type_from_string_ref();
        stable->string_equals = function_pointer_create(
            boolean, tuple_type_create(parameters, 2), tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->int_ = function_pointer_create(type_from_type(), tuple_type_create(parameters, 2),
                                               tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->integer_equals = function_pointer_create(
            boolean, tuple_type_create(parameters, 2), tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->integer_less = function_pointer_create(
            boolean, tuple_type_create(parameters, 2), tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));

        stable->integer_to_string = function_pointer_create(type_from_string_ref(), tuple_type_create(parameters, 1),
                                                            tuple_type_create(NULL, 0), optional_type_create_empty());
    }

    stable->side_effect = function_pointer_create(
        type_from_unit(), tuple_type_create(NULL, 0), tuple_type_create(NULL, 0), optional_type_create_empty());

    structure_member *globals = allocate_array(standard_library_element_count, sizeof(*globals));
    globals[0] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("type"), optional_value_create(value_from_type(type_from_type())));

    globals[1] = structure_member_create(type_from_type(), unicode_string_from_c_str("string-ref"),
                                         optional_value_create(value_from_type(type_from_string_ref())));

    globals[2] = structure_member_create(type_from_unit(), unicode_string_from_c_str("removed2"), optional_value_empty);

    globals[3] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("boolean"), optional_value_create(value_from_type(boolean)));

    globals[4] = structure_member_create(
        type_from_function_pointer(&stable->assert_), unicode_string_from_c_str("assert"), optional_value_empty);

    globals[5] = structure_member_create(type_from_function_pointer(&stable->and_), unicode_string_from_c_str("and"),
                                         optional_value_create(value_from_function_pointer(
                                             function_pointer_value_from_external(and_impl, NULL, NULL, 0))));

    globals[6] = structure_member_create(type_from_function_pointer(&stable->or_), unicode_string_from_c_str("or"),
                                         optional_value_create(value_from_function_pointer(
                                             function_pointer_value_from_external(or_impl, NULL, NULL, 0))));

    globals[7] = structure_member_create(type_from_function_pointer(&stable->not_), unicode_string_from_c_str("not"),
                                         optional_value_create(value_from_function_pointer(
                                             function_pointer_value_from_external(not_impl, NULL, NULL, 0))));

    globals[8] = structure_member_create(
        type_from_function_pointer(&stable->concat), unicode_string_from_c_str("concat"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(concat_impl, NULL, NULL, 0))));

    globals[9] = structure_member_create(
        type_from_function_pointer(&stable->string_equals), unicode_string_from_c_str("string-equals"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(string_equals_impl, NULL, NULL, 0))));

    globals[10] = structure_member_create(type_from_unit(), unicode_string_from_c_str("removed"), optional_value_empty);

    globals[11] = structure_member_create(type_from_function_pointer(&stable->int_), unicode_string_from_c_str("int"),
                                          optional_value_create(value_from_function_pointer(
                                              function_pointer_value_from_external(int_impl, NULL, NULL, 0))));

    globals[12] = structure_member_create(
        type_from_function_pointer(&stable->integer_equals), unicode_string_from_c_str("integer-equals"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(integer_equals_impl, NULL, NULL, 0))));

    globals[13] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("unit"), optional_value_create(value_from_type(type_from_unit())));

    globals[14] = structure_member_create(
        type_from_unit(), unicode_string_from_c_str("unit_value"), optional_value_create(value_from_unit()));

    globals[15] = structure_member_create(type_from_type(), unicode_string_from_c_str("option"),
                                          optional_value_create(value_from_type(type_from_enumeration(1))));

    globals[16] = structure_member_create(
        type_from_function_pointer(&stable->integer_less), unicode_string_from_c_str("integer-less"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(integer_less_impl, NULL, NULL, 0))));

    globals[17] = structure_member_create(
        type_from_function_pointer(&stable->integer_to_string), unicode_string_from_c_str("integer-to-string"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(integer_to_string_impl, NULL, NULL, 0))));

    globals[18] = structure_member_create(type_from_function_pointer(&stable->side_effect),
                                          unicode_string_from_c_str("side-effect"), optional_value_empty);

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
