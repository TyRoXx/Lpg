#include "test_stream_writer.h"
#include "lpg_stream_writer.h"
#include "test.h"

void test_stream_writer(void)
{
    {
        memory_writer m = {NULL, 0, 0};
        REQUIRE(memory_writer_equals(m, ""));
        REQUIRE(!memory_writer_equals(m, "a"));
        REQUIRE(!memory_writer_equals(m, "aa"));
    }
    {
        memory_writer m = {NULL, 0, 0};
        stream_writer s = memory_writer_erase(&m);
        REQUIRE(stream_writer_write_string(s, "abc") == success_yes);
        REQUIRE(!memory_writer_equals(m, ""));
        REQUIRE(!memory_writer_equals(m, "a"));
        REQUIRE(!memory_writer_equals(m, "ab"));
        REQUIRE(memory_writer_equals(m, "abc"));
        REQUIRE(!memory_writer_equals(m, "abcd"));
        memory_writer_free(&m);
    }
}
