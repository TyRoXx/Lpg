#include "test_unicode_string.h"
#include "test.h"
#include "lpg_unicode_string.h"
#include <string.h>

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
        unicode_code_point expected = 'a';
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
    {
        unicode_string s = unicode_string_from_c_str("ab");
        unicode_string_erase(&s, 0);
        REQUIRE(s.length == 1);
        unicode_code_point expected = 'b';
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
    {
        unicode_string s = unicode_string_from_c_str("ab");
        unicode_string_erase(&s, 1);
        REQUIRE(s.length == 1);
        unicode_code_point expected = 'a';
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
    {
        unicode_string s = unicode_string_from_c_str("");
        unicode_string_insert(&s, 0, 'a');
        REQUIRE(s.length == 1);
        unicode_code_point expected = 'a';
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
    {
        unicode_string s = unicode_string_from_c_str("b");
        unicode_string_insert(&s, 0, 'a');
        REQUIRE(s.length == 2);
        unicode_code_point const expected[] = {'a', 'b'};
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
    {
        unicode_string s = unicode_string_from_c_str("b");
        unicode_string_insert(&s, 1, 'a');
        REQUIRE(s.length == 2);
        unicode_code_point const expected[] = {'b', 'a'};
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
}
