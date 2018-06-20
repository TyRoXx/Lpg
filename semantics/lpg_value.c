#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_value.h"

implementation_ref implementation_ref_create(interface_id const target, size_t implementation_index)
{
    implementation_ref const result = {target, implementation_index};
    return result;
}

bool implementation_ref_equals(implementation_ref const left, implementation_ref const right)
{
    return (left.target == right.target) && (left.implementation_index == right.implementation_index);
}

implementation *implementation_ref_resolve(lpg_interface const *const interfaces, implementation_ref const ref)
{
    return &interfaces[ref.target].implementations[ref.implementation_index].target;
}

function_pointer_value function_pointer_value_from_external(external_function *external, void *environment,
                                                            struct value *const captures,
                                                            function_pointer const signature)
{
    function_pointer_value const result = {0, external, environment, captures, signature.captures.length, signature};
    return result;
}

function_pointer_value function_pointer_value_from_internal(function_id const code, struct value *const captures,
                                                            size_t const capture_count)
{
    function_pointer_value const result = {
        code, NULL, NULL, captures, capture_count,
        function_pointer_create(
            type_from_unit(), tuple_type_create(NULL, 0), tuple_type_create(NULL, 0), optional_type_create_empty())};
    return result;
}

bool function_pointer_value_equals(function_pointer_value const left, function_pointer_value const right)
{
    if (left.external)
    {
        if (right.external)
        {
            return (left.external == right.external) && (left.external_environment == right.external_environment);
        }
        return false;
    }
    if (right.external)
    {
        return false;
    }
    if (left.code != right.code)
    {
        return false;
    }
    if (left.capture_count != right.capture_count)
    {
        return false;
    }
    for (size_t i = 0; i < left.capture_count; ++i)
    {
        if (!value_equals(left.captures[i], right.captures[i]))
        {
            return false;
        }
    }
    return true;
}

value_tuple value_tuple_create(struct value *elements, size_t element_count)
{
    value_tuple const result = {elements, element_count};
    return result;
}

type_erased_value type_erased_value_create(implementation_ref impl, LPG_NON_NULL(struct value *self))
{
    type_erased_value const result = {impl, self};
    return result;
}

structure_value structure_value_create(struct value const *members, size_t count)
{
    ASSUME((count == 0) || (members != NULL));
    structure_value const result = {members, count};
    return result;
}

array_value array_value_create(struct value *elements, size_t count, size_t allocated, type element_type)
{
    array_value const result = {elements, count, allocated, element_type};
    return result;
}

bool array_value_equals(array_value const left, array_value const right)
{
    if (left.count != right.count)
    {
        return false;
    }
    if (left.allocated != right.allocated)
    {
        return false;
    }
    if (!type_equals(left.element_type, right.element_type))
    {
        return false;
    }
    for (size_t i = 0; i < left.count; ++i)
    {
        if (!value_equals(left.elements[i], right.elements[i]))
        {
            return false;
        }
    }
    return true;
}

value value_from_structure(structure_value const content)
{
    value result;
    result.kind = value_kind_structure;
    result.structure = content;
    return result;
}

value value_from_function_pointer(function_pointer_value pointer)
{
    value result;
    result.kind = value_kind_function_pointer;
    result.function_pointer = pointer;
    return result;
}

value value_from_string_ref(unicode_view const string_ref)
{
    value result;
    result.kind = value_kind_string;
    result.string_ref = string_ref;
    return result;
}

value value_from_unit(void)
{
    value result;
    result.kind = value_kind_unit;
    /*dummy value to avoid compiler warning*/
    result.integer_ = integer_create(0, 0);
    return result;
}

value value_from_type(type const type_)
{
    value result;
    result.kind = value_kind_type;
    result.type_ = type_;
    return result;
}

value value_from_enum_element(enum_element_id const element, type const state_type, value *const state)
{
    value result;
    result.kind = value_kind_enum_element;
    result.enum_element.which = element;
    result.enum_element.state = state;
    result.enum_element.state_type = state_type;
    return result;
}

value value_from_integer(integer const content)
{
    value result;
    result.kind = value_kind_integer;
    result.integer_ = content;
    return result;
}

value value_from_tuple(value_tuple content)
{
    value result;
    result.kind = value_kind_tuple;
    result.tuple_ = content;
    return result;
}

value value_from_enum_constructor(void)
{
    value result;
    result.kind = value_kind_enum_constructor;
    return result;
}

value value_from_type_erased(type_erased_value content)
{
    value result;
    result.kind = value_kind_type_erased;
    result.type_erased = content;
    return result;
}

value value_from_generic_enum(generic_enum_id content)
{
    value result;
    result.kind = value_kind_generic_enum;
    result.generic_enum = content;
    return result;
}

value value_from_generic_interface(generic_interface_id content)
{
    value result;
    result.kind = value_kind_generic_interface;
    result.generic_interface = content;
    return result;
}

value value_from_array(array_value *content)
{
    value result;
    result.kind = value_kind_array;
    result.array = content;
    return result;
}

value value_from_generic_lambda(generic_lambda_id content)
{
    value result;
    result.kind = value_kind_generic_lambda;
    result.generic_interface = content;
    return result;
}

value *value_allocate(value const content)
{
    value *const result = allocate(sizeof(*result));
    *result = content;
    return result;
}

value value_or_unit(value const *const maybe)
{
    return maybe ? *maybe : value_from_unit();
}

bool value_equals(value const left, value const right)
{
    ASSUME(value_is_valid(left));
    ASSUME(value_is_valid(right));
    if (left.kind != right.kind)
    {
        return false;
    }
    switch (left.kind)
    {
    case value_kind_generic_lambda:
        LPG_TO_DO();

    case value_kind_array:
        LPG_TO_DO();

    case value_kind_type_erased:
    case value_kind_pattern:
    case value_kind_generic_enum:
    case value_kind_generic_interface:
        LPG_TO_DO();

    case value_kind_integer:
        return integer_equal(left.integer_, right.integer_);

    case value_kind_string:
        return unicode_view_equals(left.string_ref, right.string_ref);

    case value_kind_function_pointer:
        return function_pointer_value_equals(left.function_pointer, right.function_pointer);

    case value_kind_structure:
        LPG_TO_DO();

    case value_kind_type:
        return type_equals(left.type_, right.type_);

    case value_kind_enum_element:
    {
        if (left.enum_element.which != right.enum_element.which)
        {
            return false;
        }
        if (!type_equals(left.enum_element.state_type, right.enum_element.state_type))
        {
            return false;
        }
        value const left_state = left.enum_element.state ? *left.enum_element.state : value_from_unit();
        value const right_state = right.enum_element.state ? *right.enum_element.state : value_from_unit();
        return value_equals(left_state, right_state);
    }

    case value_kind_unit:
        return true;

    case value_kind_tuple:
        if (left.tuple_.element_count != right.tuple_.element_count)
        {
            return false;
        }
        for (size_t i = 0; i < left.tuple_.element_count; ++i)
        {
            if (!value_equals(left.tuple_.elements[i], right.tuple_.elements[i]))
            {
                return false;
            }
        }
        return true;

    case value_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

bool value_less_than(value const left, value const right)
{
    if (left.kind != right.kind)
    {
        return left.kind < right.kind;
    }
    switch (left.kind)
    {
    case value_kind_generic_lambda:
        LPG_TO_DO();

    case value_kind_array:
        LPG_TO_DO();

    case value_kind_type_erased:
    case value_kind_pattern:
    case value_kind_generic_enum:
    case value_kind_generic_interface:
        LPG_TO_DO();

    case value_kind_integer:
        return integer_less(left.integer_, right.integer_);

    case value_kind_string:
        return unicode_view_less(left.string_ref, right.string_ref);

    case value_kind_enum_element:
        return enum_less_than(left.enum_element, right.enum_element);

    case value_kind_unit:
        return false;

    case value_kind_function_pointer:
    case value_kind_structure:
    case value_kind_type:
    case value_kind_tuple:
    case value_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

bool value_greater_than(value const left, value const right)
{
    return value_less_than(right, left);
}

optional_value optional_value_create(value v)
{
    optional_value const result = {true, v};
    return result;
}

bool enum_less_than(enum_element_value const left, enum_element_value const right)
{
    if (left.which == right.which)
    {
        value const left_value = left.state ? *left.state : value_from_unit();
        value const right_value = right.state ? *right.state : value_from_unit();
        return value_less_than(left_value, right_value);
    }
    return left.which < right.which;
}

bool value_is_valid(value const checked)
{
    return (checked.kind >= value_kind_integer) && (checked.kind <= value_kind_generic_lambda);
}

function_call_arguments function_call_arguments_create(optional_value const self, value *const arguments,
                                                       value const *globals, garbage_collector *const gc,
                                                       checked_function const *const all_functions,
                                                       lpg_interface const *all_interfaces)
{
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    function_call_arguments const result = {self, arguments, globals, gc, all_functions, all_interfaces};
    return result;
}

type get_boolean(LPG_NON_NULL(function_call_arguments const *const arguments))
{
    value const boolean = arguments->globals[3];
    ASSUME(boolean.kind == value_kind_type);
    ASSUME(boolean.type_.kind == type_kind_enumeration);
    return boolean.type_;
}

bool value_conforms_to_type(value const instance, type const expected)
{
    ASSUME(value_is_valid(instance));
    switch (expected.kind)
    {
    case type_kind_host_value:
    case type_kind_generic_lambda:
        LPG_TO_DO();

    case type_kind_unit:
        return (instance.kind == value_kind_unit);

    case type_kind_structure:
        return (instance.kind == value_kind_structure);

    case type_kind_tuple:
    {
        if (instance.kind != value_kind_tuple)
        {
            return false;
        }
        if (instance.tuple_.element_count != expected.tuple_.length)
        {
            return false;
        }
        for (size_t i = 0; i < expected.tuple_.length; ++i)
        {
            if (!value_conforms_to_type(instance.tuple_.elements[i], expected.tuple_.elements[i]))
            {
                return false;
            }
        }
        return true;
    }

    case type_kind_enumeration:
        return (instance.kind == value_kind_enum_element);

    case type_kind_string_ref:
        return (instance.kind == value_kind_string);

    case type_kind_integer_range:
        return (instance.kind == value_kind_integer) &&
               integer_range_contains_integer(expected.integer_range_, instance.integer_);

    case type_kind_lambda:
        return (instance.kind == value_kind_function_pointer);

    case type_kind_type:
        return (instance.kind == value_kind_type);

    case type_kind_interface:
        return (instance.kind == value_kind_type_erased) && (instance.type_erased.impl.target == expected.interface_);

    case type_kind_function_pointer:
    {
        if (instance.kind != value_kind_function_pointer)
        {
            return false;
        }
        if (instance.function_pointer.capture_count != expected.function_pointer_->captures.length)
        {
            return false;
        }
        for (size_t i = 0; i < instance.function_pointer.capture_count; ++i)
        {
            if (!value_conforms_to_type(
                    instance.function_pointer.captures[i], expected.function_pointer_->captures.elements[i]))
            {
                return false;
            }
        }
        return true;
    }

    case type_kind_enum_constructor:
    case type_kind_method_pointer:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}
