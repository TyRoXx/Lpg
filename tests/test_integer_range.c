#include <lpg_allocate.h>
#include "test_integer_range.h"

static void test_integer_range_size(void);
static void test_integer_range_equals(void);
static void test_integer_range_contains(void);
static void test_integer_range_list_contains(void);
static void test_integer_range_list_merge(void);
static void test_integer_range_remove(void);
static void test_integer_range_list_size(void);

void test_integer_range(void)
{
    test_integer_range_size();
    test_integer_range_equals();
    test_integer_range_contains();

    test_integer_range_list_contains();
    test_integer_range_list_merge();
    test_integer_range_list_size();
    test_integer_range_remove();
}

static void test_integer_range_list_contains(void)
{
    // Empty integer range list doesn't contain zero
    {
        integer_range_list list = integer_range_list_create(NULL, 0);
        integer_range zero = integer_range_create(integer_create(0, 0), integer_create(0, 0));

        REQUIRE(!integer_range_list_contains(list, zero));
    }
    // A list with one element contains the element
    {
        integer_range range = integer_range_create(integer_create(1, 0), integer_create(2, 0));
        integer_range_list list = integer_range_list_create_from_integer_range(range);

        REQUIRE(integer_range_list_contains(list, range));
        integer_range_list_deallocate(list);
    }

    // A list with more elements contains one
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(1, 0), integer_create(2, 0));
        elements[1] = integer_range_create(integer_create(3, 1), integer_create(4, 2));

        integer_range_list list = integer_range_list_create(elements, 2);

        REQUIRE(integer_range_list_contains(list, elements[1]));
        integer_range_list_deallocate(list);
    }

    // A list with multiple elements contains a sub-range
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(1, 0), integer_create(2, 0));
        elements[1] = integer_range_create(integer_create(3, 1), integer_create(4, 2));

        integer_range range = integer_range_create(integer_create(3, 4), integer_create(3, 1000));

        integer_range_list list = integer_range_list_create(elements, 2);

        REQUIRE(integer_range_list_contains(list, range));
        integer_range_list_deallocate(list);
    }

    // A list with multiple elements does not contain the gaps between elements
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(1, 0), integer_create(2, 0));
        elements[1] = integer_range_create(integer_create(3, 1), integer_create(4, 2));

        integer_range range = integer_range_create(integer_create(2, 0), integer_create(3, 2));

        integer_range_list list = integer_range_list_create(elements, 2);

        REQUIRE(!integer_range_list_contains(list, range));
        integer_range_list_deallocate(list);
    }
}

static void test_integer_range_equals(void)
{
    // Case 0
    {
        integer low = integer_create(0, 0);
        integer high = integer_create(0, 0);
        integer_range range1 = integer_range_create(low, high);
        integer_range range2 = integer_range_create(high, low);

        REQUIRE(integer_range_equals(range1, range2));
    }
    // Case Same upper bound
    {
        integer low1 = integer_create(0, 0);
        integer high1 = integer_create(21, 55);
        integer_range range1 = integer_range_create(low1, high1);

        integer low2 = integer_create(0, 0);
        integer high2 = integer_create(11, 11);
        integer_range range2 = integer_range_create(low2, high2);

        REQUIRE(!integer_range_equals(range1, range2));
    }
    // Case Same upper bound
    {
        integer low1 = integer_create(0, 0);
        integer high1 = integer_create(1, 0);
        integer_range range1 = integer_range_create(low1, high1);

        integer low2 = integer_create(0, 1);
        integer high2 = integer_create(1, 0);
        integer_range range2 = integer_range_create(low2, high2);

        REQUIRE(!integer_range_equals(range1, range2));
    }
    // A, B == A, B
    {
        integer low = integer_create(0, 0);
        integer high = integer_create(1, 0);
        integer_range range = integer_range_create(low, high);

        REQUIRE(integer_range_equals(range, range));
    }
    // Use case
    {
        integer low = integer_create(2, 1);
        integer high = integer_create(3, 4);
        integer_range range = integer_range_create(low, high);

        integer low1 = integer_create(2, 1);
        integer high1 = integer_create(3, 4);
        integer_range range1 = integer_range_create(low1, high1);

        REQUIRE(integer_range_equals(range, range1));
    }
}

static void test_integer_range_contains(void)
{
    integer low = integer_create(10, 10);
    integer high = integer_create(100, 100);
    integer_range default_range = integer_range_create(low, high);

    // Range contains itself
    {
        REQUIRE(integer_range_contains(default_range, default_range));
    }

    // Range does not contain 0
    {
        integer zero = integer_create(0, 0);
        integer_range zero_range = integer_range_create(zero, zero);

        REQUIRE(!integer_range_contains(default_range, zero_range));
    }
    // Range does contain sub-range
    {
        integer low1 = integer_create(11, 0);
        integer high1 = integer_create(99, 0);
        integer_range sub_range = integer_range_create(low1, high1);

        REQUIRE(integer_range_contains(default_range, sub_range));
    }

    // max too big
    {
        integer high1 = integer_create(100, 101);
        integer_range sub_range = integer_range_create(low, high1);

        REQUIRE(!integer_range_contains(default_range, sub_range));
    }
    // min too low
    {
        integer low1 = integer_create(10, 9);
        integer_range sub_range = integer_range_create(low1, high);

        REQUIRE(!integer_range_contains(default_range, sub_range));
    }
}

static void test_integer_range_size(void)
{
    // Case length 0
    {
        integer low = integer_create(0, 100);
        integer high = integer_create(0, 100);
        integer_range range = integer_range_create(low, high);

        integer expected = integer_create(0, 1);
        REQUIRE(integer_equal(integer_range_size(range), expected));
    }

    // Case length 10
    {
        integer low = integer_create(1, 90);
        integer high = integer_create(1, 100);
        integer_range range = integer_range_create(low, high);

        integer expected = integer_create(0, 11);
        REQUIRE(integer_equal(integer_range_size(range), expected));
    }

    // Case length 1 bit in high
    {
        integer low = integer_create(1, 0);
        integer high = integer_create(2, 0);
        integer_range range = integer_range_create(low, high);

        integer expected = integer_create(1, 1);
        REQUIRE(integer_equal(integer_range_size(range), expected));
    }
}

static void test_integer_range_list_merge(void)
{
    // A one element list can not be merged
    {
        integer_range range = integer_range_create(integer_create(0, 0), integer_create(0, 100));
        integer_range_list list = integer_range_list_create_from_integer_range(range);

        integer_range_list_merge(&list);

        REQUIRE(integer_range_equals(list.elements[0], range));
        REQUIRE(list.length == 1);

        integer_range_list_deallocate(list);
    }

    // A two elements which can not be merged
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(0, 0), integer_create(0, 100));
        elements[1] = integer_range_create(integer_create(100, 100), integer_create(2000, 100));

        integer_range_list list = integer_range_list_create(elements, 2);

        integer_range_list_merge(&list);

        REQUIRE(integer_range_equals(list.elements[0], elements[0]));
        REQUIRE(integer_range_equals(list.elements[1], elements[1]));
        REQUIRE(list.length == 2);

        integer_range_list_deallocate(list);
    }

    // A left merge
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(0, 0), integer_create(0, 100));
        elements[1] = integer_range_create(integer_create(0, 100), integer_create(20, 10));

        integer_range_list list = integer_range_list_create(elements, 2);

        integer_range_list_merge(&list);

        REQUIRE(
            integer_range_equals(list.elements[0], integer_range_create(integer_create(0, 0), integer_create(20, 10))));
        REQUIRE(list.length == 1);

        integer_range_list_deallocate(list);
    }

    // A right merge
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(0, 100), integer_create(20, 10));
        elements[1] = integer_range_create(integer_create(0, 0), integer_create(0, 100));

        integer_range_list list = integer_range_list_create(elements, 2);

        integer_range_list_merge(&list);

        REQUIRE(
            integer_range_equals(list.elements[0], integer_range_create(integer_create(0, 0), integer_create(20, 10))));
        REQUIRE(list.length == 1);

        integer_range_list_deallocate(list);
    }

    // Element two elements containing each other
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(0, 0), integer_create(0, 10));
        elements[1] = integer_range_create(integer_create(0, 0), integer_create(0, 5));

        integer_range_list list = integer_range_list_create(elements, 2);

        integer_range_list_merge(&list);

        REQUIRE(list.length == 1);
        REQUIRE(integer_range_equals(list.elements[0], elements[0]));

        integer_range_list_deallocate(list);
    }

    // Element two elements containing each other
    {
        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = integer_range_create(integer_create(0, 0), integer_create(0, 5));
        elements[1] = integer_range_create(integer_create(0, 0), integer_create(0, 10));

        integer_range_list list = integer_range_list_create(elements, 2);

        integer_range_list_merge(&list);

        REQUIRE(list.length == 1);
        REQUIRE(integer_range_equals(list.elements[0], elements[1]));

        integer_range_list_deallocate(list);
    }
}

void test_integer_range_remove(void)
{
    // Test remove entire element
    {
        integer_range const range = integer_range_create(integer_create(0, 0), integer_create(0, 2));

        integer_range_list list = integer_range_list_create_from_integer_range(range);

        integer_range_list_remove(&list, range);

        REQUIRE(list.length == 0);

        integer_range_list_deallocate(list);
    }

    // Test remove left part
    {
        integer const lower_bound = integer_create(0, 10);
        integer const upper_bound = integer_create(0, 20);

        integer_range const range = integer_range_create(lower_bound, upper_bound);
        integer_range_list list = integer_range_list_create_from_integer_range(range);

        integer_range const left_edge = integer_range_create(lower_bound, integer_create(0, 15));

        integer_range_list_remove(&list, left_edge);

        REQUIRE(list.length == 1);
        REQUIRE(integer_range_equals(list.elements[0], integer_range_create(integer_create(0, 16), upper_bound)));

        integer_range_list_deallocate(list);
    }

    // Test remove right part
    {
        integer const lower_bound = integer_create(0, 10);
        integer const upper_bound = integer_create(0, 20);

        integer_range const range = integer_range_create(lower_bound, upper_bound);
        integer_range_list list = integer_range_list_create_from_integer_range(range);

        integer_range const left_edge = integer_range_create(integer_create(0, 15), upper_bound);

        integer_range_list_remove(&list, left_edge);

        REQUIRE(list.length == 1);
        REQUIRE(integer_range_equals(list.elements[0], integer_range_create(lower_bound, integer_create(0, 16))));

        integer_range_list_deallocate(list);
    }

    // Test remove with integer not in range
    {
        integer const lower_bound = integer_create(0, 20);
        integer const upper_bound = integer_create(0, 30);

        integer_range const range = integer_range_create(lower_bound, upper_bound);
        integer_range_list list = integer_range_list_create_from_integer_range(range);

        // Left
        integer_range left = integer_range_create(integer_create(0, 5), integer_create(0, 10));

        integer_range_list_remove(&list, left);

        REQUIRE(list.length == 1);
        REQUIRE(integer_range_equals(list.elements[0], range));

        // Right
        integer_range const right = integer_range_create(integer_create(0, 40), integer_create(0, 50));

        integer_range_list_remove(&list, right);

        REQUIRE(list.length == 1);
        REQUIRE(integer_range_equals(list.elements[0], range));

        integer_range_list_deallocate(list);
    }

    // Test remove with element not in list
    {
        integer_range expected[] = {integer_range_create(integer_create(0, 0), integer_create(0, 100)),
                                    integer_range_create(integer_create(0, 100), integer_create(20, 10))};

        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = expected[0];
        elements[1] = expected[1];

        integer_range_list list = integer_range_list_create(elements, 2);
        integer_range element_to_remove = integer_range_create(integer_create(100, 0), integer_create(100, 100));

        integer_range_list_remove(&list, element_to_remove);

        REQUIRE(list.length == 2);
        for (size_t i = 0; i < 2; ++i)
        {
            REQUIRE(integer_range_equals(list.elements[i], expected[i]));
        }

        integer_range_list_deallocate(list);
    }
    // Test remove with element last in list exactly
    {
        integer_range expected[] = {integer_range_create(integer_create(0, 0), integer_create(0, 10)),
                                    integer_range_create(integer_create(0, 20), integer_create(0, 30))};

        integer_range *elements = allocate_array(2, sizeof(*elements));
        elements[0] = expected[0];
        elements[1] = expected[1];

        integer_range_list list = integer_range_list_create(elements, 2);
        integer_range element_to_remove = expected[1];

        integer_range_list_remove(&list, element_to_remove);

        REQUIRE(list.length == 1);
        REQUIRE(integer_range_equals(list.elements[0], expected[0]));

        integer_range_list_deallocate(list);
    }
    // Test remove with element inside list
    {
        integer_range expected[] = {
            integer_range_create(integer_create(0, 0), integer_create(0, 10)),
            integer_range_create(integer_create(0, 20), integer_create(0, 30)),
            integer_range_create(integer_create(0, 40), integer_create(0, 50)),
        };

        integer_range *elements = allocate_array(3, sizeof(*elements));
        elements[0] = expected[0];
        elements[1] = expected[1];
        elements[2] = expected[2];

        integer_range_list list = integer_range_list_create(elements, 3);
        integer_range element_to_remove = integer_range_create(integer_create(0, 20), integer_create(0, 30));

        integer_range_list_remove(&list, element_to_remove);

        REQUIRE(list.length == 2);
        REQUIRE(integer_range_equals(list.elements[0], expected[0]));
        REQUIRE(integer_range_equals(list.elements[1], expected[2]));

        integer_range_list_deallocate(list);
    }
}

static void test_integer_range_list_size(void)
{
    // Test with an empty list
    {
        integer_range_list const list = integer_range_list_create(NULL, 0);
        integer const size = integer_range_list_size(list);

        REQUIRE(integer_equal(size, integer_create(0, 0)));
    }

    // Test with one integer_range
    {
        integer const low = integer_create(0, 10);
        {
            integer_range const range = integer_range_create(low, low);

            integer_range_list const list = integer_range_list_create_from_integer_range(range);
            integer const size = integer_range_list_size(list);

            REQUIRE(integer_equal(size, integer_create(0, 1)));
            integer_range_list_deallocate(list);
        }
        {
            integer const high = integer_create(0, 12);
            integer_range const range = integer_range_create(low, high);

            integer_range_list const list = integer_range_list_create_from_integer_range(range);
            integer const size = integer_range_list_size(list);

            REQUIRE(integer_equal(size, integer_create(0, 3)));
            integer_range_list_deallocate(list);
        }
    }
    // Test with multiple integer_ranges
    {
        integer_range *const ranges = allocate_array(2, sizeof(*ranges));
        ranges[0] = integer_range_create(integer_create(0, 10), integer_create(0, 12)); // Size = 3
        ranges[1] = integer_range_create(integer_create(0, 20), integer_create(0, 22)); // Size = 3

        integer_range_list const list = integer_range_list_create(ranges, 2);
        integer const size = integer_range_list_size(list);

        REQUIRE(integer_equal(size, integer_create(0, 6)));
        integer_range_list_deallocate(list);
    }
}
