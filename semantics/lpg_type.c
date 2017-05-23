#include "lpg_type.h"
#include "lpg_for.h"
#include "lpg_allocate.h"
#include "lpg_structure_member.h"

structure structure_create(structure_member *members, struct_member_id count)
{
    structure result = {members, count};
    return result;
}

void structure_free(structure const *value)
{
    LPG_FOR(struct_member_id, i, value->count)
    {
        struct_member_free(value->members + i);
    }
    deallocate(value->members);
}

enumeration_element enumeration_element_create(unicode_string name)
{
    enumeration_element result = {name};
    return result;
}

void enumeration_element_free(enumeration_element const *value)
{
    unicode_string_free(&value->name);
}

enumeration enumeration_create(enumeration_element *elements,
                               enum_element_id size)
{
    enumeration result = {elements, size};
    return result;
}

void enumeration_free(enumeration const *value)
{
    LPG_FOR(size_t, i, value->size)
    {
        enumeration_element_free(value->elements + i);
    }
    if (value->elements)
    {
        deallocate(value->elements);
    }
}

integer_range integer_range_create(integer minimum, integer maximum)
{
    integer_range result = {minimum, maximum};
    return result;
}

type type_from_function_pointer(function_pointer const *value)
{
    type result;
    result.kind = type_kind_function_pointer;
    result.function_pointer_ = value;
    return result;
}

type type_from_unit(void)
{
    type result;
    result.kind = type_kind_unit;
    return result;
}

type type_from_string_ref(void)
{
    type result;
    result.kind = type_kind_string_ref;
    return result;
}

type type_from_enumeration(enumeration const *value)
{
    type result;
    result.kind = type_kind_enumeration;
    result.enum_ = value;
    return result;
}

type type_from_reference(type const *const referenced)
{
    type result;
    result.kind = type_kind_referenced;
    result.referenced = referenced;
    return result;
}

type type_from_type(void)
{
    type result;
    result.kind = type_kind_type;
    return result;
}

type type_from_integer_range(integer_range value)
{
    type result;
    result.kind = type_kind_integer_range;
    result.integer_range_ = value;
    return result;
}

type *type_allocate(type const value)
{
    type *const result = allocate(sizeof(*result));
    *result = value;
    return result;
}

function_pointer function_pointer_create(type result, type *arguments,
                                         size_t arity)
{
    function_pointer returned = {result, arguments, arity};
    return returned;
}

void function_pointer_free(function_pointer const *value)
{
    if (value->arguments)
    {
        deallocate(value->arguments);
    }
}
