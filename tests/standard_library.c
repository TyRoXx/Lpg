#include "standard_library.h"
#include "test.h"
#include "lpg_allocate.h"
#include <string.h>

static void standard_library_stable_free(standard_library_stable *stable)
{
    enumeration_free(&stable->boolean);
    function_pointer_free(&stable->f);
    function_pointer_free(&stable->g);
    function_pointer_free(&stable->print);
    function_pointer_free(&stable->assert_);
    function_pointer_free(&stable->and_);
    function_pointer_free(&stable->or_);
    function_pointer_free(&stable->not_);
    function_pointer_free(&stable->concat);
    function_pointer_free(&stable->type_of);
}

static value not_impl(value const *const inferred, value const *arguments,
                      garbage_collector *const gc, void *environment)
{
    (void)environment;
    (void)inferred;
    (void)gc;
    enum_element_id const argument = arguments[0].enum_element;
    return value_from_enum_element(!argument);
}

static value type_of_impl(value const *const inferred,
                          value const *const arguments,
                          garbage_collector *const gc, void *environment)
{
    (void)environment;
    (void)gc;
    value const type_of_argument = inferred[0];
    /*ignoring actual arguments*/
    (void)arguments;
    return type_of_argument;
}

static value concat_impl(value const *const inferred,
                         value const *const arguments,
                         garbage_collector *const gc, void *environment)
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

standard_library_description describe_standard_library(void)
{
    standard_library_stable *const stable = allocate(sizeof(*stable));
    {
        enumeration_element *const boolean_elements =
            allocate_array(2, sizeof(*boolean_elements));
        boolean_elements[0] =
            enumeration_element_create(unicode_string_from_c_str("false"));
        boolean_elements[1] =
            enumeration_element_create(unicode_string_from_c_str("true"));
        stable->boolean = enumeration_create(boolean_elements, 2);
    }
    type const boolean = type_from_enumeration(&stable->boolean);
    stable->f = function_pointer_create(type_from_unit(), NULL, 0);
    stable->g = function_pointer_create(type_from_unit(), NULL, 0);
    stable->print = function_pointer_create(
        type_from_unit(), type_allocate(type_from_string_ref()), 1);
    stable->assert_ =
        function_pointer_create(type_from_unit(), type_allocate(boolean), 1);
    {
        type *const and_parameters = allocate_array(2, sizeof(*and_parameters));
        and_parameters[0] = boolean;
        and_parameters[1] = boolean;
        stable->and_ = function_pointer_create(boolean, and_parameters, 2);
    }
    {
        type *const or_parameters = allocate_array(2, sizeof(*or_parameters));
        or_parameters[0] = boolean;
        or_parameters[1] = boolean;
        stable->or_ = function_pointer_create(boolean, or_parameters, 2);
    }
    {
        type *const not_parameters = allocate_array(1, sizeof(*not_parameters));
        not_parameters[0] = boolean;
        stable->not_ = function_pointer_create(boolean, not_parameters, 1);
    }
    {
        type *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = type_from_string_ref();
        parameters[1] = type_from_string_ref();
        stable->concat =
            function_pointer_create(type_from_string_ref(), parameters, 2);
    }
    {
        type *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = type_from_inferred(0);
        stable->type_of =
            function_pointer_create(type_from_type(), parameters, 1);
    }

    structure_member *globals = allocate_array(10, sizeof(*globals));
    globals[0] = structure_member_create(type_from_function_pointer(&stable->f),
                                         unicode_string_from_c_str("f"),
                                         optional_value_empty);

    globals[1] = structure_member_create(type_from_function_pointer(&stable->g),
                                         unicode_string_from_c_str("g"),
                                         optional_value_empty);

    globals[2] = structure_member_create(
        type_from_function_pointer(&stable->print),
        unicode_string_from_c_str("print"), optional_value_empty);

    globals[3] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("boolean"),
        optional_value_create(
            value_from_type(type_from_enumeration(&stable->boolean))));

    globals[4] = structure_member_create(
        type_from_function_pointer(&stable->assert_),
        unicode_string_from_c_str("assert"), optional_value_empty);

    globals[5] = structure_member_create(
        type_from_function_pointer(&stable->and_),
        unicode_string_from_c_str("and"), optional_value_empty);

    globals[6] = structure_member_create(
        type_from_function_pointer(&stable->or_),
        unicode_string_from_c_str("or"), optional_value_empty);

    globals[7] = structure_member_create(
        type_from_function_pointer(&stable->not_),
        unicode_string_from_c_str("not"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(not_impl, NULL))));

    globals[8] = structure_member_create(
        type_from_function_pointer(&stable->concat),
        unicode_string_from_c_str("concat"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(concat_impl, NULL))));

    globals[9] = structure_member_create(
        type_from_function_pointer(&stable->type_of),
        unicode_string_from_c_str("type-of"),
        optional_value_create(value_from_function_pointer(
            function_pointer_value_from_external(type_of_impl, NULL))));

    standard_library_description result = {
        structure_create(globals, 10), stable};
    return result;
}

void standard_library_description_free(
    standard_library_description const *value)
{
    structure_free(&value->globals);
    standard_library_stable_free(value->stable);
    deallocate(value->stable);
}
