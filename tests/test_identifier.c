#include "test_identifier.h"
#include "test.h"
#include "lpg_identifier.h"

static int check_identifier(char const *candidate)
{
    unicode_string s = unicode_string_from_c_str(candidate);
    int const result = is_identifier(s.data, s.length);
    unicode_string_free(&s);
    return result;
}

void test_identifier(void)
{
    REQUIRE(check_identifier("a"));
    REQUIRE(check_identifier("z"));
    REQUIRE(check_identifier("A"));
    REQUIRE(check_identifier("Z"));
    REQUIRE(check_identifier("_"));
    REQUIRE(check_identifier("a0"));
    REQUIRE(check_identifier("z1"));
    REQUIRE(check_identifier("A2"));
    REQUIRE(check_identifier("Z3"));
    REQUIRE(check_identifier("_4"));
    REQUIRE(check_identifier("a0z"));
    REQUIRE(check_identifier("z1y"));
    REQUIRE(check_identifier("A2x"));
    REQUIRE(check_identifier("Z3w"));
    REQUIRE(check_identifier("_4v"));
    REQUIRE(!check_identifier(""));
    REQUIRE(!check_identifier(" "));
    REQUIRE(!check_identifier("0"));
    REQUIRE(!check_identifier("9"));
    REQUIRE(!check_identifier("a "));
    REQUIRE(!check_identifier("z "));
    REQUIRE(!check_identifier("A "));
    REQUIRE(!check_identifier("Z "));
    REQUIRE(!check_identifier("_ "));
    REQUIRE(!check_identifier("a0 "));
    REQUIRE(!check_identifier("z1 "));
    REQUIRE(!check_identifier("A 2"));
    REQUIRE(!check_identifier("Z 3"));
    REQUIRE(!check_identifier("_ 4"));
    REQUIRE(!check_identifier("a 0z"));
    REQUIRE(!check_identifier("z1 y"));
    REQUIRE(!check_identifier("A2 x"));
    REQUIRE(!check_identifier("Z3 w"));
    REQUIRE(!check_identifier("_4 v"));
}
