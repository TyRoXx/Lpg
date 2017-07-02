#include "test_implicitly_convertible.h"
#include "lpg_check.h"
#include "test.h"

void test_integer_convertible(){
    type small_range = type_from_integer_range(integer_range_create(integer_create(20, 10), integer_create(30, 7)));
    type big_range = type_from_integer_range(integer_range_create(integer_create(0, 0), integer_create(700, 0)));
    REQUIRE(is_implicitly_convertible(small_range, big_range));
    REQUIRE(!is_implicitly_convertible(big_range, small_range));
}

void test_implicitly_convertible(){
    test_integer_convertible();
}
