#include <lpg_allocate.h>
#include <lpg_type.h>
#include <lpg_assert.h>
#include "test_tuple.h"

void test_tuple(void) {
    type *elements = allocate_array(2, sizeof(type));
    elements[0] = type_from_unit();
    elements[1] = type_from_integer_range(integer_range_create(integer_create(1, 3), integer_create(20, 1)));
    tuple_type tuple1 = {elements, 2};
    type t1 = type_from_tuple_type(&tuple1);
    {
        tuple_type tuple2 = {elements, 3};
        type t2 = type_from_tuple_type(&tuple2);
        ASSERT(!type_equals(t1, t2));
    }
    {
        tuple_type tuple2 = {elements, 2};
        type t2 = type_from_tuple_type(&tuple2);
        ASSERT(type_equals(t1, t2));
    }

}
