#pragma once

#include "lpg_ecmascript_value.h"
#include "lpg_enum_id.h"
#include "lpg_type.h"
#include <stddef.h>

typedef enum enum_encoding_element_stateful_encoding {
    enum_encoding_element_stateful_encoding_indirect,
    enum_encoding_element_stateful_encoding_direct
} enum_encoding_element_stateful_encoding;

typedef struct enum_encoding_element_stateful
{
    enum_encoding_element_stateful_encoding encoding;
    union
    {
        ecmascript_value_type direct;
        ecmascript_value indirect;
    };
} enum_encoding_element_stateful;

enum_encoding_element_stateful enum_encoding_element_stateful_from_direct(ecmascript_value_type direct);
enum_encoding_element_stateful enum_encoding_element_stateful_from_indirect(ecmascript_value indirect);

typedef struct enum_encoding_element_stateless
{
    ecmascript_value key;
} enum_encoding_element_stateless;

enum_encoding_element_stateless enum_encoding_element_stateless_create(ecmascript_value key);

typedef union enum_encoding_element
{
    enum_encoding_element_stateful stateful;
    enum_encoding_element_stateless stateless;
} enum_encoding_element;

enum_encoding_element enum_encoding_element_from_stateful(enum_encoding_element_stateful stateful);
enum_encoding_element enum_encoding_element_from_stateless(enum_encoding_element_stateless stateless);

typedef struct enum_encoding_strategy
{
    enum_encoding_element *elements;
} enum_encoding_strategy;

enum_encoding_strategy enum_encoding_strategy_create(enum_encoding_element *elements);
void enum_encoding_strategy_free(enum_encoding_strategy const freed);

typedef struct enum_encoding_strategy_cache
{
    enumeration const *all_enums;
    enum_encoding_strategy *entries;
    enum_id entry_count;
} enum_encoding_strategy_cache;

enum_encoding_strategy_cache enum_encoding_strategy_cache_create(enumeration const *all_enums);
void enum_encoding_strategy_cache_free(enum_encoding_strategy_cache const freed);
enum_encoding_strategy *enum_encoding_strategy_cache_require(enum_encoding_strategy_cache *cache,
                                                             enum_id const required);
