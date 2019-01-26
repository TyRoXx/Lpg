#include "lpg_standard_library.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include <string.h>

static void standard_library_stable_free(standard_library_stable *stable)
{
    function_pointer_free(&stable->assert_);
    function_pointer_free(&stable->not_);
    function_pointer_free(&stable->concat);
    function_pointer_free(&stable->string_equals);
    function_pointer_free(&stable->int_);
    function_pointer_free(&stable->integer_equals);
    function_pointer_free(&stable->integer_less);
    function_pointer_free(&stable->integer_to_string);
    function_pointer_free(&stable->type_equals);
    function_pointer_free(&stable->side_effect);
    function_pointer_free(&stable->subtract);
    function_pointer_free(&stable->add);
    function_pointer_free(&stable->add_u32);
    function_pointer_free(&stable->add_u64);
    function_pointer_free(&stable->and_u64);
    function_pointer_free(&stable->or_u64);
    function_pointer_free(&stable->xor_u64);
    function_pointer_free(&stable->not_u64);
    function_pointer_free(&stable->shift_left_u64);
    function_pointer_free(&stable->shift_right_u64);
}

external_function_result not_impl(value const *const captures, void *environment, optional_value const self,
                                  value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)environment;
    (void)captures;
    enum_element_id const argument = arguments[0].enum_element.which;
    return external_function_result_from_success(value_from_enum_element(!argument, type_from_unit(), NULL));
}

external_function_result concat_impl(value const *const captures, void *environment, optional_value const self,
                                     value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    unicode_view const left = arguments[0].string;
    unicode_view const right = arguments[1].string;
    size_t const result_length = left.length + right.length;
    char *const result = garbage_collector_try_allocate(context->gc, result_length);
    if (!result)
    {
        return external_function_result_create_out_of_memory();
    }
    memcpy(result, left.begin, left.length);
    memcpy(result + left.length, right.begin, right.length);
    return external_function_result_from_success(value_from_string(unicode_view_create(result, result_length)));
}

external_function_result string_equals_impl(value const *const captures, void *environment, optional_value const self,
                                            value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)environment;
    (void)captures;
    unicode_view const left = arguments[0].string;
    unicode_view const right = arguments[1].string;
    return external_function_result_from_success(
        value_from_enum_element(unicode_view_equals(left, right), type_from_unit(), NULL));
}

external_function_result type_equals_impl(value const *const captures, void *environment, optional_value const self,
                                          value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)environment;
    (void)captures;
    type const left = arguments[0].type_;
    type const right = arguments[1].type_;
    return external_function_result_from_success(
        value_from_enum_element(type_equals(left, right), type_from_unit(), NULL));
}

external_function_result int_impl(value const *const captures, void *environment, optional_value const self,
                                  value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)environment;
    (void)captures;
    integer const first = arguments[0].integer_;
    integer const second = arguments[1].integer_;
    if (integer_less(first, second))
    {
        return external_function_result_from_success(
            value_from_type(type_from_integer_range(integer_range_create(first, second))));
    }
    return external_function_result_from_success(
        value_from_type(type_from_integer_range(integer_range_create(second, first))));
}

external_function_result fail_impl(value const *const captures, void *environment, optional_value const self,
                                   value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)arguments;
    (void)environment;
    (void)captures;
    LPG_TO_DO();
}

external_function_result integer_equals_impl(value const *const captures, void *environment, optional_value const self,
                                             value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)environment;
    (void)captures;
    integer const left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    return external_function_result_from_success(
        value_from_enum_element(integer_equal(left, right), type_from_unit(), NULL));
}

external_function_result integer_less_impl(value const *const captures, void *environment, optional_value const self,
                                           value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)environment;
    (void)captures;
    integer const left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    return external_function_result_from_success(
        value_from_enum_element(integer_less(left, right), type_from_unit(), NULL));
}

external_function_result integer_to_string_impl(value const *const captures, void *environment,
                                                optional_value const self, value *const arguments,
                                                interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;

    integer const left = arguments[0].integer_;
    const unsigned int printing_base = 10;
    const size_t buffer_max_size = 20;
    char *buffer = garbage_collector_allocate(context->gc, buffer_max_size);
    unicode_view const buffer_begin = integer_format(left, lower_case_digits, printing_base, buffer, buffer_max_size);
    return external_function_result_from_success(value_from_string(buffer_begin));
}

external_function_result subtract_impl(value const *const captures, void *environment, optional_value const self,
                                       value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    integer const left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    integer_difference const difference = integer_subtract(left, right);
    if (difference.is_positive)
    {
        value *const state = garbage_collector_allocate(context->gc, sizeof(*state));
        *state = value_from_integer(difference.value_if_positive);
        return external_function_result_from_success(
            value_from_enum_element(0, type_from_integer_range(integer_range_max()), state));
    }
    return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
}

external_function_result add_impl(value const *const captures, void *environment, optional_value const self,
                                  value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    integer left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    if (integer_add(&left, right))
    {
        value *const state = garbage_collector_allocate(context->gc, sizeof(*state));
        *state = value_from_integer(left);
        return external_function_result_from_success(
            value_from_enum_element(0, type_from_integer_range(integer_range_max()), state));
    }
    return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
}

external_function_result add_u32_impl(value const *const captures, void *environment, optional_value const self,
                                      value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    integer left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    if (integer_add(&left, right) && integer_less_or_equals(left, integer_create(0, UINT32_MAX)))
    {
        value *const state = garbage_collector_allocate(context->gc, sizeof(*state));
        *state = value_from_integer(left);
        return external_function_result_from_success(
            value_from_enum_element(0, type_from_integer_range(integer_range_max()), state));
    }
    return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
}

external_function_result add_u64_impl(value const *const captures, void *environment, optional_value const self,
                                      value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    integer left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    if (integer_add(&left, right) && integer_less_or_equals(left, integer_create(0, UINT64_MAX)))
    {
        value *const state = garbage_collector_allocate(context->gc, sizeof(*state));
        *state = value_from_integer(left);
        return external_function_result_from_success(
            value_from_enum_element(0, type_from_integer_range(integer_range_max()), state));
    }
    return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
}

static external_function_result and_u64_impl(value const *const captures, void *environment, optional_value const self,
                                             value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    (void)context;
    integer left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    return external_function_result_from_success(value_from_integer(integer_create(0, (right.low & left.low))));
}

static external_function_result or_u64_impl(value const *const captures, void *environment, optional_value const self,
                                            value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    (void)context;
    integer left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    return external_function_result_from_success(value_from_integer(integer_create(0, (right.low | left.low))));
}

static external_function_result xor_u64_impl(value const *const captures, void *environment, optional_value const self,
                                             value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    (void)context;
    integer left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    return external_function_result_from_success(value_from_integer(integer_create(0, (right.low ^ left.low))));
}

static external_function_result not_u64_impl(value const *const captures, void *environment, optional_value const self,
                                             value *const arguments, interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    (void)context;
    integer const input = arguments[0].integer_;
    return external_function_result_from_success(value_from_integer(integer_create(0, ~input.low)));
}

static external_function_result shift_left_u64_impl(value const *const captures, void *environment,
                                                    optional_value const self, value *const arguments,
                                                    interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    (void)context;
    integer const left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    ASSUME(right.low < 64);
    return external_function_result_from_success(value_from_integer(integer_create(0, (left.low << right.low))));
}

static external_function_result shift_right_u64_impl(value const *const captures, void *environment,
                                                     optional_value const self, value *const arguments,
                                                     interpreter *const context)
{
    (void)self;
    (void)environment;
    (void)captures;
    (void)context;
    integer const left = arguments[0].integer_;
    integer const right = arguments[1].integer_;
    ASSUME(right.low < 64);
    return external_function_result_from_success(value_from_integer(integer_create(0, (left.low >> right.low))));
}

external_function_result side_effect_impl(value const *const captures, void *environment, optional_value const self,
                                          value *const arguments, interpreter *const context)
{
    (void)self;
    (void)context;
    (void)arguments;
    (void)captures;
    (void)environment;
    return external_function_result_from_success(value_from_unit());
}

standard_library_description describe_standard_library(void)
{
    standard_library_stable *const stable = allocate(sizeof(*stable));

    type const boolean = type_from_enumeration(standard_library_enum_boolean);
    type const subtract_result = type_from_enumeration(standard_library_enum_subtract_result);
    type const add_result = type_from_enumeration(standard_library_enum_add_result);
    type const add_u32_result = type_from_enumeration(standard_library_enum_add_u32_result);
    type const add_u64_result = type_from_enumeration(standard_library_enum_add_u64_result);

    stable->assert_ = function_pointer_create(optional_type_create_set(type_from_unit()),
                                              tuple_type_create(type_allocate(boolean), 1), tuple_type_create(NULL, 0),
                                              optional_type_create_empty());
    {
        type *const not_parameters = allocate_array(1, sizeof(*not_parameters));
        not_parameters[0] = boolean;
        stable->not_ = function_pointer_create(optional_type_create_set(boolean), tuple_type_create(not_parameters, 1),
                                               tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_string();
        parameters[1] = type_from_string();
        stable->concat =
            function_pointer_create(optional_type_create_set(type_from_string()), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_string();
        parameters[1] = type_from_string();
        stable->string_equals =
            function_pointer_create(optional_type_create_set(boolean), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->int_ =
            function_pointer_create(optional_type_create_set(type_from_type()), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->integer_equals =
            function_pointer_create(optional_type_create_set(boolean), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));
        parameters[1] = parameters[0];
        stable->integer_less =
            function_pointer_create(optional_type_create_set(boolean), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max()));

        stable->integer_to_string =
            function_pointer_create(optional_type_create_set(type_from_string()), tuple_type_create(parameters, 1),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_max());
        parameters[1] = parameters[0];
        stable->subtract =
            function_pointer_create(optional_type_create_set(subtract_result), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_integer_range(integer_range_max());
        parameters[1] = parameters[0];
        stable->add = function_pointer_create(optional_type_create_set(add_result), tuple_type_create(parameters, 2),
                                              tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT32_MAX)));
        parameters[1] = parameters[0];
        stable->add_u32 =
            function_pointer_create(optional_type_create_set(add_u32_result), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX)));
        parameters[1] = parameters[0];
        stable->add_u64 =
            function_pointer_create(optional_type_create_set(add_u64_result), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX)));
        parameters[1] = parameters[0];
        stable->and_u64 =
            function_pointer_create(optional_type_create_set(parameters[0]), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX)));
        parameters[1] = parameters[0];
        stable->or_u64 =
            function_pointer_create(optional_type_create_set(parameters[0]), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX)));
        parameters[1] = parameters[0];
        stable->xor_u64 =
            function_pointer_create(optional_type_create_set(parameters[0]), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX)));
        stable->not_u64 =
            function_pointer_create(optional_type_create_set(parameters[0]), tuple_type_create(parameters, 1),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX)));
        parameters[1] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, 63)));
        stable->shift_left_u64 =
            function_pointer_create(optional_type_create_set(parameters[0]), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX)));
        parameters[1] = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(0, 63)));
        stable->shift_right_u64 =
            function_pointer_create(optional_type_create_set(parameters[0]), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }

    stable->side_effect =
        function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                tuple_type_create(NULL, 0), optional_type_create_empty());

    stable->fail = function_pointer_create(optional_type_create_empty(), tuple_type_create(NULL, 0),
                                           tuple_type_create(NULL, 0), optional_type_create_empty());

    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_type();
        parameters[1] = type_from_type();
        stable->type_equals =
            function_pointer_create(optional_type_create_set(boolean), tuple_type_create(parameters, 2),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
    }

    structure_member *globals = allocate_array(standard_library_element_count, sizeof(*globals));
    globals[0] = structure_member_create(type_from_function_pointer(&stable->side_effect),
                                         unicode_string_from_c_str("side_effect"), optional_value_empty);

    globals[1] = structure_member_create(
        type_from_function_pointer(&stable->integer_to_string), unicode_string_from_c_str("integer_to_string"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(integer_to_string_impl, NULL, NULL, stable->integer_to_string))));

    globals[2] = structure_member_create(
        type_from_function_pointer(&stable->type_equals), unicode_string_from_c_str("type_equals"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(type_equals_impl, NULL, NULL, stable->type_equals))));

    globals[3] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("boolean"), optional_value_create(value_from_type(boolean)));

    globals[4] = structure_member_create(
        type_from_function_pointer(&stable->assert_), unicode_string_from_c_str("assert"), optional_value_empty);

    globals[5] = structure_member_create(
        type_from_function_pointer(&stable->integer_less), unicode_string_from_c_str("integer_less"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(integer_less_impl, NULL, NULL, stable->integer_less))));

    globals[6] = structure_member_create(
        type_from_function_pointer(&stable->integer_equals), unicode_string_from_c_str("integer_equals"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(integer_equals_impl, NULL, NULL, stable->integer_equals))));

    globals[7] = structure_member_create(
        type_from_function_pointer(&stable->not_), unicode_string_from_c_str("not"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(not_impl, NULL, NULL, stable->not_))));

    globals[8] =
        structure_member_create(type_from_function_pointer(&stable->concat), unicode_string_from_c_str("concat"),
                                optional_value_create(value_from_function_pointer(
                                    function_pointer_value_from_external(concat_impl, NULL, NULL, stable->concat))));

    globals[9] = structure_member_create(
        type_from_function_pointer(&stable->string_equals), unicode_string_from_c_str("string_equals"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(string_equals_impl, NULL, NULL, stable->string_equals))));

    globals[10] = structure_member_create(
        type_from_function_pointer(&stable->int_), unicode_string_from_c_str("int"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(int_impl, NULL, NULL, stable->int_))));

    globals[11] = structure_member_create(type_from_type(), unicode_string_from_c_str("host_value"),
                                          optional_value_create(value_from_type(type_from_host_value())));

    globals[12] = structure_member_create(
        type_from_function_pointer(&stable->fail), unicode_string_from_c_str("fail"), optional_value_empty);

    globals[13] = structure_member_create(type_from_type(), unicode_string_from_c_str("subtract_result"),
                                          optional_value_create(value_from_type(subtract_result)));

    globals[14] =
        structure_member_create(type_from_function_pointer(&stable->subtract), unicode_string_from_c_str("subtract"),
                                optional_value_create(value_from_function_pointer(function_pointer_value_from_external(
                                    subtract_impl, NULL, NULL, stable->subtract))));

    globals[15] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("add_result"), optional_value_create(value_from_type(add_result)));

    globals[16] = structure_member_create(
        type_from_function_pointer(&stable->add), unicode_string_from_c_str("add"),
        optional_value_create(
            value_from_function_pointer(function_pointer_value_from_external(add_impl, NULL, NULL, stable->add))));

    globals[17] = structure_member_create(type_from_type(), unicode_string_from_c_str("add_u32_result"),
                                          optional_value_create(value_from_type(add_u32_result)));

    globals[18] =
        structure_member_create(type_from_function_pointer(&stable->add_u32), unicode_string_from_c_str("add_u32"),
                                optional_value_create(value_from_function_pointer(
                                    function_pointer_value_from_external(add_u32_impl, NULL, NULL, stable->add_u32))));

    globals[19] = structure_member_create(type_from_type(), unicode_string_from_c_str("add_u64_result"),
                                          optional_value_create(value_from_type(add_u64_result)));

    globals[20] =
        structure_member_create(type_from_function_pointer(&stable->add_u64), unicode_string_from_c_str("add_u64"),
                                optional_value_create(value_from_function_pointer(
                                    function_pointer_value_from_external(add_u64_impl, NULL, NULL, stable->add_u64))));

    globals[21] =
        structure_member_create(type_from_function_pointer(&stable->and_u64), unicode_string_from_c_str("and_u64"),
                                optional_value_create(value_from_function_pointer(
                                    function_pointer_value_from_external(and_u64_impl, NULL, NULL, stable->and_u64))));

    globals[22] =
        structure_member_create(type_from_function_pointer(&stable->or_u64), unicode_string_from_c_str("or_u64"),
                                optional_value_create(value_from_function_pointer(
                                    function_pointer_value_from_external(or_u64_impl, NULL, NULL, stable->or_u64))));

    globals[23] =
        structure_member_create(type_from_function_pointer(&stable->xor_u64), unicode_string_from_c_str("xor_u64"),
                                optional_value_create(value_from_function_pointer(
                                    function_pointer_value_from_external(xor_u64_impl, NULL, NULL, stable->xor_u64))));

    globals[24] =
        structure_member_create(type_from_function_pointer(&stable->not_u64), unicode_string_from_c_str("not_u64"),
                                optional_value_create(value_from_function_pointer(
                                    function_pointer_value_from_external(not_u64_impl, NULL, NULL, stable->not_u64))));

    globals[25] = structure_member_create(
        type_from_function_pointer(&stable->shift_left_u64), unicode_string_from_c_str("shift_left_u64"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(shift_left_u64_impl, NULL, NULL, stable->shift_left_u64))));

    globals[26] = structure_member_create(
        type_from_function_pointer(&stable->shift_right_u64), unicode_string_from_c_str("shift_right_u64"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(shift_right_u64_impl, NULL, NULL, stable->shift_right_u64))));

    LPG_STATIC_ASSERT(standard_library_element_count == 27);

    standard_library_description const result = {structure_create(globals, standard_library_element_count), stable};
    return result;
}

void standard_library_description_free(standard_library_description const *freed)
{
    structure_free(&freed->globals);
    standard_library_stable_free(freed->stable);
    deallocate(freed->stable);
}
