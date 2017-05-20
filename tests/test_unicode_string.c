#include "test_unicode_string.h"
#include "test.h"
#include "lpg_unicode_string.h"
#include <string.h>
#include <stdio.h>
#include <lpg_string_literal.h>

void test_unicode_string(void)
{
    {
        unicode_string s = unicode_string_from_c_str("");
        REQUIRE(s.length == 0);
        unicode_string_free(&s);
    }
    {
        unicode_string s = unicode_string_from_c_str("a");
        REQUIRE(s.length == 1);
        char const expected = 'a';
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
}
