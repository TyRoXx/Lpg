#include "test_value.h"
#include "test.h"
#include "lpg_value.h"

void test_value(void)
{
    REQUIRE(!value_equals(
        value_from_unit(), value_from_string_ref(unicode_view_from_c_str(""))));
    {
        value element = value_from_unit();
        REQUIRE(
            !value_equals(value_from_tuple(value_tuple_create(NULL, 0)),
                          value_from_tuple(value_tuple_create(&element, 1))));
    }
    {
        value first = value_from_unit();
        value second = value_from_unit();
        REQUIRE(value_equals(value_from_tuple(value_tuple_create(&first, 1)),
                             value_from_tuple(value_tuple_create(&second, 1))));
    }
    {
        value first = value_from_unit();
        value second = value_from_string_ref(unicode_view_from_c_str(""));
        REQUIRE(
            !value_equals(value_from_tuple(value_tuple_create(&first, 1)),
                          value_from_tuple(value_tuple_create(&second, 1))));
    }
	REQUIRE(!value_equals(
		value_from_enum_element(0,NULL), value_from_enum_element(1,NULL)));
}
