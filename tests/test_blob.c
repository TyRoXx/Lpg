#include "test_blob.h"
#include "test.h"
#include "lpg_blob.h"
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
}
