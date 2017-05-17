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

void type_free(type const *value)
{
    switch (value->kind)
    {
    case type_kind_structure:
        structure_free(&value->structure_);
        break;

    case type_kind_function_pointer:
        function_pointer_free(&value->function_pointer_);
        break;

    case type_kind_unit:
        break;

    case type_kind_string_ref:
        break;

    case type_kind_enumeration:
        enumeration_free(&value->enum_);
        break;

    case type_kind_referenced:
        break;

    case type_kind_type:
        break;
    }
}

void type_deallocate(type *value)
{
    type_free(value);
    deallocate(value);
}

type type_from_function_pointer(function_pointer value)
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

type type_from_enumeration(enumeration const value)
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

type *type_allocate(type const value)
{
    type *const result = allocate(sizeof(*result));
    *result = value;
    return result;
}

function_pointer function_pointer_create(type *result, type *arguments,
                                         size_t arity)
{
    function_pointer returned = {result, arguments, arity};
    return returned;
}

void function_pointer_free(function_pointer const *value)
{
    type_deallocate(value->result);
    LPG_FOR(size_t, i, value->arity)
    {
        type_free(value->arguments + i);
    }
    if (value->arguments)
    {
        deallocate(value->arguments);
    }
}
