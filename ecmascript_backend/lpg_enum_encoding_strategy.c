#include "lpg_enum_encoding_strategy.h"
#include "lpg_allocate.h"
#include "lpg_ecmascript_value_set.h"

enum_encoding_element_stateful enum_encoding_element_stateful_from_direct(ecmascript_value_set direct)
{
    ASSUME(!ecmascript_value_set_is_empty(direct));
    enum_encoding_element_stateful result;
    result.encoding = enum_encoding_element_stateful_encoding_direct;
    result.direct = direct;
    return result;
}

enum_encoding_element_stateful enum_encoding_element_stateful_from_indirect(void)
{
    enum_encoding_element_stateful result;
    result.encoding = enum_encoding_element_stateful_encoding_indirect;
    return result;
}

enum_encoding_element_stateless enum_encoding_element_stateless_create(ecmascript_value key)
{
    enum_encoding_element_stateless const result = {key};
    return result;
}

enum_encoding_element enum_encoding_element_from_stateful(enum_encoding_element_stateful stateful)
{
    enum_encoding_element result;
    result.has_state = true;
    result.stateful = stateful;
    return result;
}

enum_encoding_element enum_encoding_element_from_stateless(enum_encoding_element_stateless stateless)
{
    enum_encoding_element result;
    result.has_state = false;
    result.stateless = stateless;
    return result;
}

ecmascript_value_set enum_encoding_element_value_set(enum_encoding_element const from)
{
    if (from.has_state)
    {
        switch (from.stateful.encoding)
        {
        case enum_encoding_element_stateful_encoding_direct:
            return from.stateful.direct;

        case enum_encoding_element_stateful_encoding_indirect:
            return ecmascript_value_set_create_array();
        }
        LPG_UNREACHABLE();
    }
    return ecmascript_value_set_create_from_value(from.stateless.key);
}

enum_encoding_strategy enum_encoding_strategy_create(enum_encoding_element *elements, enum_element_id count)
{
    enum_encoding_strategy const result = {elements, count};
    return result;
}

void enum_encoding_strategy_free(enum_encoding_strategy const freed)
{
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

ecmascript_value_set enum_encoding_strategy_value_set(enum_encoding_strategy const from)
{
    ecmascript_value_set result = ecmascript_value_set_create_empty();
    for (size_t i = 0; i < from.count; ++i)
    {
        ASSERT(ecmascript_value_set_merge_without_intersection(
            &result, enum_encoding_element_value_set(from.elements[i])));
    }
    return result;
}

enum_encoding_strategy_cache enum_encoding_strategy_cache_create(enumeration const *all_enums,
                                                                 enum_id const number_of_enums)
{
    enum_encoding_strategy_cache const result = {
        all_enums, allocate_array(number_of_enums, sizeof(*result.entries)), number_of_enums};
    for (enum_id i = 0; i < number_of_enums; ++i)
    {
        result.entries[i] = enum_encoding_strategy_create(NULL, 0);
    }
    return result;
}

void enum_encoding_strategy_cache_free(enum_encoding_strategy_cache const freed)
{
    for (size_t i = 0; i < freed.entry_count; ++i)
    {
        enum_encoding_strategy_free(freed.entries[i]);
    }
    if (freed.entries)
    {
        deallocate(freed.entries);
    }
}

static void set_stateful_enum_fallback_strategy(enum_encoding_strategy *const strategy, enumeration const description)
{
    for (enum_element_id i = 0; i < description.size; ++i)
    {
        enumeration_element const element = description.elements[i];
        if (element.state.is_set)
        {
            strategy->elements[i] = enum_encoding_element_from_stateful(enum_encoding_element_stateful_from_indirect());
        }
        else
        {
            strategy->elements[i] = enum_encoding_element_from_stateless(
                enum_encoding_element_stateless_create(ecmascript_value_create_integer(i)));
        }
    }
}

static ecmascript_value_set how_is_type_encoded(enum_encoding_strategy_cache *const cache, type const what)
{
    switch (what.kind)
    {
    case type_kind_enum_constructor:
        return ecmascript_value_set_create_function();

    case type_kind_string:
        return ecmascript_value_set_create_string();

    case type_kind_integer_range:
    {
        // TODO: do this correctly
        ASSUME(what.integer_range_.minimum.high == 0);
        uint64_t max = 9007199254740991;
        return ecmascript_value_set_create_integer_range(ecmascript_integer_range_create(
            what.integer_range_.minimum.low,
            what.integer_range_.maximum.low < max ? what.integer_range_.maximum.low : max));
    }

    case type_kind_enumeration:
    {
        enum_encoding_strategy const *const strategy = enum_encoding_strategy_cache_require(cache, what.enum_);
        return enum_encoding_strategy_value_set(*strategy);
    }

    case type_kind_interface:
        return ecmascript_value_set_create_object();

    case type_kind_host_value:
        return ecmascript_value_set_create_anything();

    case type_kind_structure:
    case type_kind_tuple:
        return ecmascript_value_set_create_array();

    case type_kind_unit:
    case type_kind_type:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
    case type_kind_generic_lambda:
    case type_kind_generic_struct:
        return ecmascript_value_set_create_undefined();

    case type_kind_function_pointer:
    case type_kind_lambda:
        return ecmascript_value_set_create_function();

    case type_kind_method_pointer:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static void define_strategy(enum_encoding_strategy *const strategy, enum_encoding_strategy_cache *cache,
                            enum_id const defining)
{
    ASSUME(!strategy->elements);
    ASSUME(cache);
    enumeration const description = cache->all_enums[defining];
    strategy->elements = allocate_array(description.size, sizeof(*strategy->elements));
    strategy->count = description.size;
    if (has_stateful_element(description))
    {
        ecmascript_value_set used_values = ecmascript_value_set_create_empty();
        bool use_fallback = false;
        for (enum_element_id i = 0; i < description.size; ++i)
        {
            enumeration_element const element = description.elements[i];
            if (!element.state.is_set)
            {
                continue;
            }
            ecmascript_value_set const state_encoding = how_is_type_encoded(cache, element.state.value);
            ASSUME(!ecmascript_value_set_is_empty(state_encoding));
            if (!ecmascript_value_set_merge_without_intersection(&used_values, state_encoding))
            {
                use_fallback = true;
                break;
            }
            strategy->elements[i] =
                enum_encoding_element_from_stateful(enum_encoding_element_stateful_from_direct(state_encoding));
        }
        if (!use_fallback)
        {
            for (enum_element_id i = 0; i < description.size; ++i)
            {
                enumeration_element const element = description.elements[i];
                if (element.state.is_set)
                {
                    continue;
                }
                if (!ecmascript_value_set_merge_without_intersection(
                        &used_values, ecmascript_value_set_create_integer_range(ecmascript_integer_range_create(i, i))))
                {
                    use_fallback = true;
                    break;
                }
                strategy->elements[i] = enum_encoding_element_from_stateless(
                    enum_encoding_element_stateless_create(ecmascript_value_create_integer(i)));
            }
        }
        if (use_fallback)
        {
            set_stateful_enum_fallback_strategy(strategy, description);
        }
    }
    else
    {
        if (description.size == 2)
        {
            strategy->elements[0] = enum_encoding_element_from_stateless(
                enum_encoding_element_stateless_create(ecmascript_value_create_boolean(false)));
            strategy->elements[1] = enum_encoding_element_from_stateless(
                enum_encoding_element_stateless_create(ecmascript_value_create_boolean(true)));
        }
        else
        {
            for (enum_element_id i = 0; i < description.size; ++i)
            {
                strategy->elements[i] = enum_encoding_element_from_stateless(
                    enum_encoding_element_stateless_create(ecmascript_value_create_integer(i)));
            }
        }
    }
}

enum_encoding_strategy *enum_encoding_strategy_cache_require(enum_encoding_strategy_cache *cache,
                                                             enum_id const required)
{
    ASSUME(cache);
    ASSUME(required < cache->entry_count);
    enum_encoding_strategy *const strategy = cache->entries + required;
    enumeration const target = cache->all_enums[required];
    if ((target.size == 0) || strategy->elements)
    {
        return strategy;
    }
    define_strategy(strategy, cache, required);
    ASSUME(strategy->elements);
    return strategy;
}
