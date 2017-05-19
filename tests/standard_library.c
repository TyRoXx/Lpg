#include "standard_library.h"
#include "test.h"
#include "lpg_allocate.h"

standard_library_description describe_standard_library(void)
{
    structure_member *globals = allocate_array(8, sizeof(*globals));
    globals[0] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()), NULL, 0)),
        unicode_string_from_c_str("f"), optional_value_empty);

    globals[1] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()), NULL, 0)),
        unicode_string_from_c_str("g"), optional_value_empty);

    globals[2] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()),
                                    type_allocate(type_from_string_ref()), 1)),
        unicode_string_from_c_str("print"), optional_value_empty);

    enumeration_element *const boolean_elements =
        allocate_array(2, sizeof(*boolean_elements));
    boolean_elements[0] =
        enumeration_element_create(unicode_string_from_c_str("false"));
    boolean_elements[1] =
        enumeration_element_create(unicode_string_from_c_str("true"));
    type *const boolean_type = type_allocate(
        type_from_enumeration(enumeration_create(boolean_elements, 2)));

    globals[3] = structure_member_create(
        type_from_type(), unicode_string_from_c_str("boolean"),
        optional_value_create(value_from_type(boolean_type)));

    globals[4] = structure_member_create(
        type_from_function_pointer(function_pointer_create(
            type_allocate(type_from_unit()),
            type_allocate(type_from_reference(boolean_type)), 1)),
        unicode_string_from_c_str("assert"), optional_value_empty);

    {
        type *const and_parameters = allocate_array(2, sizeof(*and_parameters));
        and_parameters[0] = type_from_reference(boolean_type);
        and_parameters[1] = type_from_reference(boolean_type);
        globals[5] = structure_member_create(
            type_from_function_pointer(function_pointer_create(
                type_allocate(type_from_reference(boolean_type)),
                and_parameters, 2)),
            unicode_string_from_c_str("and"), optional_value_empty);
    }

    {
        type *const and_parameters = allocate_array(2, sizeof(*and_parameters));
        and_parameters[0] = type_from_reference(boolean_type);
        and_parameters[1] = type_from_reference(boolean_type);
        globals[6] = structure_member_create(
            type_from_function_pointer(function_pointer_create(
                type_allocate(type_from_reference(boolean_type)),
                and_parameters, 2)),
            unicode_string_from_c_str("or"), optional_value_empty);
    }

    {
        type *const and_parameters = allocate_array(1, sizeof(*and_parameters));
        and_parameters[0] = type_from_reference(boolean_type);
        globals[7] = structure_member_create(
            type_from_function_pointer(function_pointer_create(
                type_allocate(type_from_reference(boolean_type)),
                and_parameters, 1)),
            unicode_string_from_c_str("not"), optional_value_empty);
    }

    standard_library_description result = {
        structure_create(globals, 8), boolean_type};
    return result;
}

void standard_library_description_free(
    standard_library_description const *value)
{
    structure_free(&value->globals);
    type_deallocate(value->boolean);
}
