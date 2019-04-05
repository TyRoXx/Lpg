#include "lpg_ecmascript_value_set.h"

ecmascript_integer_range ecmascript_integer_range_create(uint64_t first, uint64_t last)
{
    ASSUME(last <= 9007199254740991ull);
    ecmascript_integer_range const result = {first, last + 1};
    return result;
}

ecmascript_integer_range ecmascript_integer_range_create_empty(void)
{
    ecmascript_integer_range const result = {0, 0};
    return result;
}

ecmascript_integer_range ecmascript_integer_range_create_any(void)
{
    return ecmascript_integer_range_create(0, 9007199254740991ull);
}

bool ecmascript_integer_range_is_empty(ecmascript_integer_range const range)
{
    return (range.first == range.after_last);
}

bool ecmascript_integer_range_equals(ecmascript_integer_range const left, ecmascript_integer_range const right)
{
    if (ecmascript_integer_range_is_empty(left) && ecmascript_integer_range_is_empty(right))
    {
        return true;
    }
    return (left.first == right.first) && (left.after_last == right.after_last);
}

static uint64_t minimum(uint64_t const first, uint64_t const second)
{
    return (first < second) ? first : second;
}

static uint64_t maximum(uint64_t const first, uint64_t const second)
{
    return (first > second) ? first : second;
}

bool ecmascript_integer_range_merge_without_intersection(ecmascript_integer_range *const into,
                                                         ecmascript_integer_range const from)
{
    uint64_t const new_min = minimum(into->first, from.first);
    uint64_t const new_max = maximum(into->after_last - 1, from.after_last - 1);
    uint64_t const new_size = (new_max - new_min);
    uint64_t const into_size = (into->after_last - into->first);
    uint64_t const from_size = (from.after_last - from.first);
    if (new_size < (into_size + from_size))
    {
        return false;
    }
    into->first = new_min;
    into->after_last = new_max + 1;
    return true;
}

ecmascript_value_set ecmascript_value_set_create(ecmascript_integer_range integers, bool undefined, bool null,
                                                 bool string, bool array, bool object, bool function, bool bool_true,
                                                 bool bool_false)
{
    ecmascript_value_set const result = {
        integers, undefined, null, string, array, object, function, bool_true, bool_false};
    return result;
}

ecmascript_value_set ecmascript_value_set_create_empty(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), false, false, false, false, false, false, false, false);
}

ecmascript_value_set ecmascript_value_set_create_function(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), false, false, false, false, false, true, false, false);
}

ecmascript_value_set ecmascript_value_set_create_string(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), false, false, true, false, false, false, false, false);
}

ecmascript_value_set ecmascript_value_set_create_integer_range(ecmascript_integer_range range)
{
    return ecmascript_value_set_create(range, false, false, false, false, false, false, false, false);
}

ecmascript_value_set ecmascript_value_set_create_array(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), false, false, false, true, false, false, false, false);
}

ecmascript_value_set ecmascript_value_set_create_undefined(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), true, false, false, false, false, false, false, false);
}

ecmascript_value_set ecmascript_value_set_create_from_value(ecmascript_value const from)
{
    switch (from.type)
    {
    case ecmascript_value_boolean:
        return ecmascript_value_set_create_bool(from.boolean);

    case ecmascript_value_integer:
        return ecmascript_value_set_create_integer_range(ecmascript_integer_range_create(from.integer, from.integer));

    case ecmascript_value_null:
    case ecmascript_value_undefined:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

ecmascript_value_set ecmascript_value_set_create_bool(bool value)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), false, false, false, false, false, false, value, !value);
}

ecmascript_value_set ecmascript_value_set_create_any_bool(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), false, false, false, false, false, false, true, true);
}

ecmascript_value_set ecmascript_value_set_create_object(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_empty(), false, false, false, false, true, false, false, false);
}

ecmascript_value_set ecmascript_value_set_create_anything(void)
{
    return ecmascript_value_set_create(
        ecmascript_integer_range_create_any(), true, true, true, true, true, true, true, true);
}

static bool merge_booleans(bool *const into, bool const from)
{
    if (*into && from)
    {
        return false;
    }
    if (from)
    {
        *into = from;
    }
    return true;
}

bool ecmascript_value_set_merge_without_intersection(ecmascript_value_set *into, ecmascript_value_set const other)
{
    if (ecmascript_integer_range_is_empty(into->integer))
    {
        into->integer = other.integer;
    }
    else if (!ecmascript_integer_range_merge_without_intersection(&into->integer, other.integer))
    {
        return false;
    }
    return merge_booleans(&into->undefined, other.undefined) && merge_booleans(&into->null, other.null) &&
           merge_booleans(&into->string, other.string) && merge_booleans(&into->array, other.array) &&
           merge_booleans(&into->object, other.object) && merge_booleans(&into->function, other.function) &&
           merge_booleans(&into->bool_true, other.bool_true) && merge_booleans(&into->bool_false, other.bool_false);
}

bool ecmascript_value_set_is_empty(ecmascript_value_set const checked)
{
    return !checked.bool_true && !checked.bool_false && !checked.array && !checked.function &&
           ecmascript_integer_range_is_empty(checked.integer) && !checked.null && !checked.object && !checked.string &&
           !checked.undefined;
}

bool ecmascript_value_set_equals(ecmascript_value_set const left, ecmascript_value_set const right)
{
    return (left.array == right.array) && (left.undefined == right.undefined) && (left.null == right.null) &&
           (left.string == right.string) && (left.object == right.object) && (left.function == right.function) &&
           (left.bool_true == right.bool_true) && (left.bool_false == right.bool_false) &&
           ecmascript_integer_range_equals(left.integer, right.integer);
}
