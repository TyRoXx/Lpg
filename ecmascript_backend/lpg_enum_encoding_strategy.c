#include "lpg_enum_encoding_strategy.h"
#include "lpg_allocate.h"

enum_encoding_element_stateful enum_encoding_element_stateful_from_direct(ecmascript_value_type direct)
{
    enum_encoding_element_stateful result;
    result.encoding = enum_encoding_element_stateful_encoding_direct;
    result.direct = direct;
    return result;
}

enum_encoding_element_stateful enum_encoding_element_stateful_from_indirect(ecmascript_value indirect)
{
    enum_encoding_element_stateful result;
    result.encoding = enum_encoding_element_stateful_encoding_indirect;
    result.indirect = indirect;
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
    result.stateful = stateful;
    return result;
}

enum_encoding_element enum_encoding_element_from_stateless(enum_encoding_element_stateless stateless)
{
    enum_encoding_element result;
    result.stateless = stateless;
    return result;
}

enum_encoding_strategy enum_encoding_strategy_create(enum_encoding_element *elements)
{
    enum_encoding_strategy const result = {elements};
    return result;
}

void enum_encoding_strategy_free(enum_encoding_strategy const freed)
{
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

enum_encoding_strategy_cache enum_encoding_strategy_cache_create(enumeration const *all_enums)
{
    enum_encoding_strategy_cache const result = {all_enums, NULL, 0};
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

static void define_strategy(enum_encoding_strategy *const strategy, enum_encoding_strategy_cache *cache,
                            enum_id const defining)
{
    ASSUME(!strategy->elements);
    ASSUME(cache);
    enumeration const description = cache->all_enums[defining];
    strategy->elements = allocate_array(description.size, sizeof(*strategy->elements));
    if (has_stateful_element(description))
    {
        for (enum_element_id i = 0; i < description.size; ++i)
        {
            enumeration_element const element = description.elements[i];
            if (element.state.is_set)
            {
                // TODO: support direct representation
                strategy->elements[i] = enum_encoding_element_from_stateful(
                    enum_encoding_element_stateful_from_indirect(ecmascript_value_create_integer(i)));
            }
            else
            {
                strategy->elements[i] = enum_encoding_element_from_stateless(
                    enum_encoding_element_stateless_create(ecmascript_value_create_integer(i)));
            }
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
    if (required >= cache->entry_count)
    {
        cache->entries = reallocate_array(cache->entries, (required + 1), sizeof(*cache->entries));
        for (enum_id i = cache->entry_count; i <= required; ++i)
        {
            cache->entries[i] = enum_encoding_strategy_create(NULL);
        }
        cache->entry_count = (required + 1);
    }
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
