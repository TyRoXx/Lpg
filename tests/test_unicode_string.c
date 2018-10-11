#include "test_unicode_string.h"
#include "lpg_unicode_string.h"
#include "lpg_unicode_view.h"
#include "test.h"
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
        char const expected = 'a';
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        unicode_string_free(&s);
    }
    {
        unicode_string s = unicode_string_from_c_str("abc");
        unicode_string_free(&s);
    }
    {
        blob const raw = blob_from_c_str("a");
        raw.data[0] = -1;
        unicode_string validated = unicode_string_validate(raw);
        REQUIRE(unicode_view_equals_c_str(unicode_view_from_string(validated), ""));
        unicode_string_free(&validated);
    }
    {
        blob const raw = blob_from_c_str("aaa");
        raw.data[1] = -1;
        unicode_string validated = unicode_string_validate(raw);
        REQUIRE(unicode_view_equals_c_str(unicode_view_from_string(validated), "a"));
        unicode_string_free(&validated);
    }
    {
        blob const raw = blob_from_c_str("aaa");
        unicode_string validated = unicode_string_validate(raw);
        REQUIRE(unicode_view_equals_c_str(unicode_view_from_string(validated), "aaa"));
        unicode_string_free(&validated);
    }
}
