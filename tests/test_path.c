#include "test_path.h"
#include "test.h"
#include "lpg_path.h"
#include "lpg_array_size.h"

void test_path(void)
{
    REQUIRE(unicode_view_equals_c_str(path_remove_leaf(unicode_view_from_c_str("")), ""));
    REQUIRE(unicode_view_equals_c_str(path_remove_leaf(unicode_view_from_c_str("a")), ""));
    REQUIRE(unicode_view_equals_c_str(path_remove_leaf(unicode_view_from_c_str("/")), "/"));
    REQUIRE(unicode_view_equals_c_str(path_remove_leaf(unicode_view_from_c_str("/home/test")), "/home"));
    REQUIRE(unicode_view_equals_c_str(path_remove_leaf(unicode_view_from_c_str("/home/test/")), "/home"));
    REQUIRE(unicode_view_equals_c_str(path_remove_leaf(path_remove_leaf(unicode_view_from_c_str("/home/test"))), "/"));
    {
        unicode_view const pieces[] = {unicode_view_from_c_str("a")};
        unicode_string const combined = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
        REQUIRE(unicode_view_equals_c_str(unicode_view_from_string(combined), "a"));
        unicode_string_free(&combined);
    }
    {
        unicode_view const pieces[] = {unicode_view_from_c_str("a"), unicode_view_from_c_str("b")};
        unicode_string const combined = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
        REQUIRE(unicode_view_equals_c_str(unicode_view_from_string(combined), "a/b"));
        unicode_string_free(&combined);
    }
    {
        unicode_view const pieces[] = {unicode_view_from_c_str("a\\b"), unicode_view_from_c_str("c")};
        unicode_string const combined = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
        REQUIRE(unicode_view_equals_c_str(unicode_view_from_string(combined), "a/b/c"));
        unicode_string_free(&combined);
    }
}
