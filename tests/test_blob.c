#include "test_blob.h"
#include "lpg_blob.h"
#include "test.h"
#include <string.h>

void test_blob(void)
{
    {
        blob s = blob_from_c_str("");
        REQUIRE(s.length == 0);
        blob_free(&s);
    }
    {
        blob s = blob_from_c_str("a");
        REQUIRE(s.length == 1);
        char const expected = 'a';
        REQUIRE(!memcmp(s.data, &expected, sizeof(expected)));
        blob_free(&s);
    }

    {
        blob blob_range = blob_from_range("hello", 5);
        REQUIRE(blob_range.length == 5);
        blob blob_string = blob_from_c_str("hello");
        REQUIRE(blob_equals(blob_range, blob_string));
        blob_free(&blob_range);
        blob_free(&blob_string);
    }
}
