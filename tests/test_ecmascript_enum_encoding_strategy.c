#include "test_ecmascript_enum_encoding_strategy.h"
#include "lpg_array_size.h"
#include "lpg_enum_encoding_strategy.h"
#include "test.h"

void test_ecmascript_enum_encoding_strategy(void)
{
    {
        ecmascript_value_set used_values =
            ecmascript_value_set_create_integer_range(ecmascript_integer_range_create(0, 10));
        REQUIRE(ecmascript_value_set_merge_without_intersection(&used_values, ecmascript_value_set_create_undefined()));
        ecmascript_value_set expected = ecmascript_value_set_create_empty();
        expected.undefined = true;
        expected.integer = ecmascript_integer_range_create(0, 10);
        REQUIRE(ecmascript_value_set_equals(expected, used_values));
    }

    // stateless 1 element
    {
        enumeration all_enums[1];
        enumeration_element elements[1];
        elements[0] = enumeration_element_create(unicode_string_from_c_str("a"), optional_type_create_empty());
        all_enums[0] = enumeration_create(elements, LPG_ARRAY_SIZE(elements));
        enum_encoding_strategy_cache cache = enum_encoding_strategy_cache_create(all_enums, LPG_ARRAY_SIZE(all_enums));
        enum_encoding_strategy const *const result = enum_encoding_strategy_cache_require(&cache, 0);
        REQUIRE(result);
        REQUIRE(result->count == LPG_ARRAY_SIZE(elements));
        for (size_t i = 0; i < LPG_ARRAY_SIZE(elements); ++i)
        {
            enum_encoding_element const expected = enum_encoding_element_from_stateless(
                enum_encoding_element_stateless_create(ecmascript_value_create_integer(i)));
            enum_encoding_element const got = result->elements[i];
            REQUIRE(enum_encoding_element_equals(expected, got));
        }
        enum_encoding_strategy_cache_free(cache);
        enumeration_element_free(elements + 0);
    }
    // stateless 2 elements
    {
        enumeration all_enums[1];
        enumeration_element elements[2];
        elements[0] = enumeration_element_create(unicode_string_from_c_str("a"), optional_type_create_empty());
        elements[1] = enumeration_element_create(unicode_string_from_c_str("b"), optional_type_create_empty());
        all_enums[0] = enumeration_create(elements, LPG_ARRAY_SIZE(elements));
        enum_encoding_strategy_cache cache = enum_encoding_strategy_cache_create(all_enums, LPG_ARRAY_SIZE(all_enums));
        enum_encoding_strategy const *const result = enum_encoding_strategy_cache_require(&cache, 0);
        REQUIRE(result);
        REQUIRE(result->count == LPG_ARRAY_SIZE(elements));
        REQUIRE(enum_encoding_element_equals(
            enum_encoding_element_from_stateless(
                enum_encoding_element_stateless_create(ecmascript_value_create_boolean(false))),
            result->elements[0]));
        REQUIRE(enum_encoding_element_equals(
            enum_encoding_element_from_stateless(
                enum_encoding_element_stateless_create(ecmascript_value_create_boolean(true))),
            result->elements[1]));
        enum_encoding_strategy_cache_free(cache);
        enumeration_element_free(elements + 0);
        enumeration_element_free(elements + 1);
    }
    // stateless 3 elements
    {
        enumeration all_enums[1];
        enumeration_element elements[3];
        elements[0] = enumeration_element_create(unicode_string_from_c_str("a"), optional_type_create_empty());
        elements[1] = enumeration_element_create(unicode_string_from_c_str("b"), optional_type_create_empty());
        elements[2] = enumeration_element_create(unicode_string_from_c_str("c"), optional_type_create_empty());
        all_enums[0] = enumeration_create(elements, LPG_ARRAY_SIZE(elements));
        enum_encoding_strategy_cache cache = enum_encoding_strategy_cache_create(all_enums, LPG_ARRAY_SIZE(all_enums));
        enum_encoding_strategy const *const result = enum_encoding_strategy_cache_require(&cache, 0);
        REQUIRE(result);
        REQUIRE(result->count == LPG_ARRAY_SIZE(elements));
        for (size_t i = 0; i < LPG_ARRAY_SIZE(elements); ++i)
        {
            enum_encoding_element const expected = enum_encoding_element_from_stateless(
                enum_encoding_element_stateless_create(ecmascript_value_create_integer(i)));
            enum_encoding_element const got = result->elements[i];
            REQUIRE(enum_encoding_element_equals(expected, got));
        }
        enum_encoding_strategy_cache_free(cache);
        enumeration_element_free(elements + 0);
        enumeration_element_free(elements + 1);
        enumeration_element_free(elements + 2);
    }
    // stateful 2 elements with integer (integer first)
    {
        enumeration all_enums[1];
        enumeration_element elements[2];
        elements[0] = enumeration_element_create(
            unicode_string_from_c_str("a"), optional_type_create_set(type_from_integer_range(
                                                integer_range_create(integer_create(0, 0), integer_create(0, 10)))));
        elements[1] = enumeration_element_create(unicode_string_from_c_str("b"), optional_type_create_empty());
        all_enums[0] = enumeration_create(elements, LPG_ARRAY_SIZE(elements));
        enum_encoding_strategy_cache cache = enum_encoding_strategy_cache_create(all_enums, LPG_ARRAY_SIZE(all_enums));
        enum_encoding_strategy const *const result = enum_encoding_strategy_cache_require(&cache, 0);
        REQUIRE(result);
        REQUIRE(result->count == LPG_ARRAY_SIZE(elements));
        REQUIRE(enum_encoding_element_equals(
            enum_encoding_element_from_stateful(enum_encoding_element_stateful_from_direct(
                ecmascript_value_set_create_integer_range(ecmascript_integer_range_create(0, 10)))),
            result->elements[0]));
        {
            enum_encoding_element const got = result->elements[1];
            REQUIRE(enum_encoding_element_equals(
                enum_encoding_element_from_stateless(
                    enum_encoding_element_stateless_create(ecmascript_value_create_undefined())),
                got));
        }
        enum_encoding_strategy_cache_free(cache);
        enumeration_element_free(elements + 0);
        enumeration_element_free(elements + 1);
    }
    // stateful 2 elements with integer (integer second)
    {
        enumeration all_enums[1];
        enumeration_element elements[2];
        elements[0] = enumeration_element_create(unicode_string_from_c_str("a"), optional_type_create_empty());
        elements[1] = enumeration_element_create(
            unicode_string_from_c_str("b"), optional_type_create_set(type_from_integer_range(
                                                integer_range_create(integer_create(0, 0), integer_create(0, 10)))));
        all_enums[0] = enumeration_create(elements, LPG_ARRAY_SIZE(elements));
        enum_encoding_strategy_cache cache = enum_encoding_strategy_cache_create(all_enums, LPG_ARRAY_SIZE(all_enums));
        enum_encoding_strategy const *const result = enum_encoding_strategy_cache_require(&cache, 0);
        REQUIRE(result);
        REQUIRE(result->count == LPG_ARRAY_SIZE(elements));
        {
            enum_encoding_element const got = result->elements[0];
            REQUIRE(enum_encoding_element_equals(
                enum_encoding_element_from_stateless(
                    enum_encoding_element_stateless_create(ecmascript_value_create_undefined())),
                got));
        }
        REQUIRE(enum_encoding_element_equals(
            enum_encoding_element_from_stateful(enum_encoding_element_stateful_from_direct(
                ecmascript_value_set_create_integer_range(ecmascript_integer_range_create(0, 10)))),
            result->elements[1]));
        enum_encoding_strategy_cache_free(cache);
        enumeration_element_free(elements + 0);
        enumeration_element_free(elements + 1);
    }
}
