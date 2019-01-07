#include "lpg_integer_range.h"
#include <lpg_allocate.h>

integer_range integer_range_create(integer const minimum, integer const maximum)
{
    integer_range const result = {minimum, maximum};
    return result;
}

integer_range integer_range_create_u64(void)
{
    return integer_range_create(integer_create(0, 0), integer_create(0, UINT64_MAX));
}

integer_range integer_range_max(void)
{
    return integer_range_create(integer_create(0, 0), integer_max());
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
    integer_difference const difference = integer_subtract(value.maximum, value.minimum);
    ASSUME(difference.is_positive);
    integer range_size_zero_based = difference.value_if_positive;
    ASSUME(integer_add(&range_size_zero_based, integer_create(0, 1)));
    return range_size_zero_based;
}

bool integer_range_list_overlaps(integer_range_list const first, integer_range const second)
{
    for (size_t i = 0; i < first.length; ++i)
    {
        if (integer_range_overlaps(first.elements[i], second))
        {
            return true;
        }
    }
    return false;
}

bool integer_range_contains(integer_range const haystack, integer_range const needle)
{
    return integer_less_or_equals(haystack.minimum, needle.minimum) &&
           integer_less_or_equals(needle.maximum, haystack.maximum);
}

bool integer_range_overlaps(integer_range const first, integer_range const second)
{
    if (integer_less_or_equals(first.maximum, second.maximum) && integer_less_or_equals(second.minimum, first.maximum))
    {
        return true;
    }
    if (integer_less_or_equals(second.maximum, first.maximum) && integer_less_or_equals(first.minimum, second.maximum))
    {
        return true;
    }
    return false;
}

bool integer_range_contains_integer(const integer_range haystack, integer const needle)
{
    // haystack.min <= needle && needle <= haystack.max
    return integer_less_or_equals(haystack.minimum, needle) && integer_less_or_equals(needle, haystack.maximum);
}

integer_range integer_range_combine(integer_range const left, integer_range const right)
{
    integer minimum = left.minimum;
    if (integer_less(right.minimum, minimum))
    {
        minimum = right.minimum;
    }
    integer maximum = left.maximum;
    if (integer_less(maximum, right.maximum))
    {
        maximum = right.maximum;
    }
    return integer_range_create(minimum, maximum);
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

static void remove_element_from_integer_range_list(integer_range_list *const range_list, size_t index)
{
    ASSUME(range_list->length > 0);
    ASSUME(index < range_list->length);
    integer_range last_element = range_list->elements[range_list->length - 1];
    range_list->elements[index] = last_element;
    range_list->length--;
}

void integer_range_list_remove(integer_range_list *const haystack, integer_range const needle)
{
    for (size_t i = 0; i < haystack->length; ++i)
    {
        const integer_range element = haystack->elements[i];
        integer_range needle_copy = needle;
        // Full remove
        if (integer_range_equals(element, needle))
        {
            remove_element_from_integer_range_list(haystack, i);
            i--;
        }
        /*
         * NOTE: if the integer overflows when adding one then it will not be in the
         * range anymore
         *
         * Remove left edge:
         *  needle.max inside element && needle.min <= element.min
         */
        else if (integer_range_contains_integer(element, needle.maximum) &&
                 integer_less_or_equals(needle.minimum, element.minimum))
        {
            bool not_overflown = integer_add(&needle_copy.maximum, integer_create(0, 1));
            ASSUME(not_overflown);
            haystack->elements[i].minimum = needle_copy.maximum;
        }
        /*
         * NOTE: if the integer overflows when adding one then it will not be in the
         * range anymore
         *
         * Remove right edge:
         *   needle.min inside element && needle.max >= element.max
         */
        else if (integer_range_contains_integer(element, needle.minimum) &&
                 integer_less_or_equals(element.maximum, needle.maximum))
        {
            bool not_overflown = integer_add(&needle_copy.minimum, integer_create(0, 1));
            ASSUME(not_overflown);
            haystack->elements[i].maximum = needle_copy.minimum;
        }
        else if (integer_range_contains(element, needle))
        {
            bool not_overflown = integer_add(&needle_copy.maximum, integer_create(0, 1));
            ASSUME(not_overflown);
            integer_difference const difference = integer_subtract(needle.minimum, integer_create(0, 1));
            ASSUME(difference.is_positive);
            needle_copy.minimum = difference.value_if_positive;
            haystack->elements[i].maximum = needle_copy.minimum;

            haystack->elements = reallocate(haystack->elements, haystack->length + 1);
            haystack->elements[haystack->length] = integer_range_create(needle_copy.maximum, element.maximum);
            haystack->length++;
        }
    }
}

void integer_range_list_merge(integer_range_list *const unmerged_list)
{
    for (size_t i = 0; i < unmerged_list->length; ++i)
    {
        integer_range first = unmerged_list->elements[i];
        for (size_t j = i + 1; j < unmerged_list->length;)
        {
            integer_range second = unmerged_list->elements[j];
            if (integer_range_contains(first, second))
            {
                remove_element_from_integer_range_list(unmerged_list, j);
            }
            else if (integer_range_contains(second, first))
            {
                remove_element_from_integer_range_list(unmerged_list, i);
                j = i + 1;
            }
            else if (integer_range_contains_integer(first, second.minimum))
            {
                unmerged_list->elements[i].maximum = integer_maximum(first.maximum, second.maximum);
                remove_element_from_integer_range_list(unmerged_list, j);
            }
            else if (integer_range_contains_integer(first, second.maximum))
            {
                unmerged_list->elements[i].minimum = integer_minimum(first.minimum, second.minimum);
                remove_element_from_integer_range_list(unmerged_list, j);
            }
            else
            {
                j++;
            }
        }
    }
}

integer integer_range_list_size(integer_range_list const list)
{
    integer result = integer_create(0, 0);
    for (size_t i = 0; i < list.length; ++i)
    {
        bool ok = integer_add(&result, integer_range_size(list.elements[i]));
        ASSUME(ok); // Because the range sizes can't be bigger than the integer range
    }
    return result;
}
