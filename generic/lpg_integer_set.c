#include "lpg_integer_set.h"
#include "lpg_allocate.h"

integer_set integer_set_from_range(integer_range const range)
{
    integer const size = integer_range_size(range);
    if (!integer_less_or_equals(size, integer_create(0, SIZE_MAX)))
    {
        LPG_TO_DO();
    }
    integer_set const result = {allocate_array(size.low, sizeof(*result.elements)), size.low};
    for (uint64_t i = 0; i < size.low; ++i)
    {
        result.elements[i] = range.minimum;
        ASSERT(integer_add(result.elements + i, integer_create(0, i)));
    }
    return result;
}

void integer_set_free(integer_set const freed)
{
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

bool integer_set_contains_range(integer_set const set, integer_range const range)
{
    bool added = true;
    for (integer i = range.minimum; integer_less_or_equals(i, range.maximum);
         added = integer_add(&i, integer_create(0, 1)))
    {
        ASSUME(added);
        if (!integer_set_contains(set, i))
        {
            return false;
        }
    }
    return true;
}

bool integer_set_contains(integer_set const set, integer const element)
{
    for (size_t i = 0; i < set.used; ++i)
    {
        if (integer_equal(set.elements[i], element))
        {
            return true;
        }
    }
    return false;
}

void integer_set_remove_range(integer_set *const set, integer_range const range)
{
    bool added = true;
    for (integer i = range.minimum; integer_less_or_equals(i, range.maximum);
         added = integer_add(&i, integer_create(0, 1)))
    {
        ASSUME(added);
        integer_set_remove(set, i);
    }
}

void integer_set_remove(integer_set *const set, integer const element)
{
    for (size_t i = 0; i < set->used; ++i)
    {
        if (integer_equal(set->elements[i], element))
        {
            set->elements[i] = set->elements[set->used - 1];
            set->used -= 1;
            return;
        }
    }
}

bool integer_set_is_empty(integer_set const set)
{
    return (set.used == 0);
}
