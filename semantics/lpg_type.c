#include "lpg_type.h"
#include "lpg_for.h"
#include "lpg_allocate.h"
#include "lpg_structure_member.h"
#include "lpg_assert.h"

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

bool tuple_type_equals(tuple_type const left, tuple_type const right)
{
    if (left.length != right.length)
    {
        return false;
    }
    for (size_t i = 0; i < left.length; ++i)
    {
        if (!type_equals(left.elements[i], right.elements[i]))
        {
            return false;
        }
    }
    return true;
}

integer_range integer_range_create(integer minimum, integer maximum)
{
    integer_range result = {minimum, maximum};
    return result;
}

bool integer_range_equals(integer_range const left, integer_range const right)
{
    return integer_equal(left.minimum, right.minimum) &&
           integer_equal(left.maximum, right.maximum);
}

bool function_pointer_equals(function_pointer const left,
                             function_pointer const right)
{
    if (!type_equals(left.result, right.result))
    {
        return false;
    }
    if (left.arity != right.arity)
    {
        return false;
    }
    for (size_t i = 0; i < left.arity; ++i)
    {
        if (!type_equals(left.arguments[i], right.arguments[i]))
        {
            return false;
        }
    }
    return true;
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

type type_from_tuple_type(tuple_type const *value)
{
    type result;
    result.kind = type_kind_tuple;
    result.tuple_ = value;
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

type type_from_inferred(size_t const inferred)
{
    type result;
    result.kind = type_kind_inferred;
    result.inferred = inferred;
    return result;
}

type *type_allocate(type const value)
{
    type *const result = allocate(sizeof(*result));
    *result = value;
    return result;
}

bool type_equals(type const left, type const right)
{
    if (left.kind != right.kind)
    {
        return false;
    }
    switch (left.kind)
    {
    case type_kind_structure:
        LPG_TO_DO();

    case type_kind_function_pointer:
        if (!type_equals(left.function_pointer_->result,
                         right.function_pointer_->result))
        {
            return false;
        }
        if (left.function_pointer_->arity != right.function_pointer_->arity)
        {
            return false;
        }
        for (size_t i = 0; i < left.function_pointer_->arity; ++i)
        {
            if (!type_equals(left.function_pointer_->arguments[i],
                             right.function_pointer_->arguments[i]))
            {
                return false;
            }
        }
        return true;

    case type_kind_unit:
        return true;

    case type_kind_string_ref:
        return true;

    case type_kind_enumeration:
        return (left.enum_ == right.enum_);

    case type_kind_tuple:
        return tuple_type_equals(*left.tuple_, *right.tuple_);

    case type_kind_type:
        return true;

    case type_kind_integer_range:
        return integer_range_equals(left.integer_range_, right.integer_range_);

    case type_kind_inferred:
        LPG_TO_DO();
    }
    UNREACHABLE();
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
