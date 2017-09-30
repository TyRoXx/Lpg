#include <lpg_string_literal.h>
#include <string.h>
#include "test_decode_string_literal.h"
#include "test.h"

static decode_string_literal_result
expect_decoded_content(char const *string, char const *expected_content)
{
    memory_writer m = {NULL, 0, 0};
    stream_writer const s = memory_writer_erase(&m);
    unicode_view const uv = unicode_view_create(string, strlen(string));
    decode_string_literal_result const result = decode_string_literal(uv, s);
    REQUIRE(memory_writer_equals(m, expected_content));
    memory_writer_free(&m);
    return result;
}

void test_decode_string_literal(void)
{
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello\"", "Hello");
        REQUIRE(result.is_valid);
        REQUIRE(result.length == 7);
    }
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello \n World\"", "Hello ");
        REQUIRE(!result.is_valid);
        REQUIRE(result.length == 7);
    }
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello \\\\ World\"", "Hello \\ World");
        REQUIRE(result.is_valid);
        REQUIRE(result.length == 16);
    }
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello \\ World\"", "Hello ");
        REQUIRE(!result.is_valid);
        REQUIRE(result.length == 8);
    }
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello \\\" World\"", "Hello \" World");
        REQUIRE(result.is_valid);
        REQUIRE(result.length == 16);
    }
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello \\n World\"", "Hello \n World");
        REQUIRE(result.is_valid);
        REQUIRE(result.length == 16);
    }
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello \\t World\"", "Hello \t World");
        REQUIRE(result.is_valid);
        REQUIRE(result.length == 16);
    }
    {
        decode_string_literal_result result =
            expect_decoded_content("\"Hello \' World\"", "Hello \' World");
        REQUIRE(result.is_valid);
        REQUIRE(result.length == 15);
    }
}
