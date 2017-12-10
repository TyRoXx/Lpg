#include <lpg_allocate.h>
#include "lpg_integer_range.h"

integer_range integer_range_create(integer minimum, integer maximum)
{
    integer_range const result = {minimum, maximum};
    return result;
}

bool integer_range_equals(integer_range const left, integer_range const right)
{
    return integer_equal(left.minimum, right.minimum) && integer_equal(left.maximum, right.maximum);
}

integer_range_list integer_range_list_create_from_integer_range(integer_range const element)
{
    integer_range *elements = allocate_array(1, sizeof(*elements));
    elements[0] = element;
    return integer_range_list_create(elements, 1);
}

integer_range_list integer_range_list_create(integer_range *elements, size_t const length)
{
    integer_range_list result = {elements, length};
    return result;
}

void integer_range_list_deallocate(integer_range_list value)
{
    deallocate(value.elements);
}

integer integer_range_size(integer_range const value)
{
    integer range_size_zero_based = integer_subtract(value.maximum, value.minimum);
    ASSUME(integer_add(&range_size_zero_based, integer_create(0, 1)));
    return range_size_zero_based;
}

bool integer_range_contains(integer_range const haystack, integer_range const needle)
{
    return integer_less_or_equals(haystack.minimum, needle.minimum) &&
           integer_less_or_equals(needle.maximum, haystack.maximum);
}

bool integer_range_list_contains(integer_range_list const haystack, integer_range const needle)
{
    for (size_t i = 0; i < haystack.length; ++i)
    {
        if (integer_range_contains(haystack.elements[i], needle))
        {
            return true;
        }
    }
    return false;
}

static integer_range *integer_range_remove(integer_range from, integer_range what)
{

    (void)from;
    (void)what;
    return NULL;
}

integer_range_list integer_range_list_remove(integer_range_list haystack, integer_range const needle)
{
    for (size_t i = 0; i < haystack.length; ++i)
    {
        const integer_range *const elements = haystack.elements;
        if (integer_range_contains(elements[i], needle))
        {
            integer_range_remove(elements[i], needle);
        }
    }
    return haystack;
}