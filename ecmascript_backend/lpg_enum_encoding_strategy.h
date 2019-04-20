#pragma once

#include "lpg_ecmascript_value_set.h"
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
    ecmascript_value_set direct;
} enum_encoding_element_stateful;

enum_encoding_element_stateful enum_encoding_element_stateful_from_direct(ecmascript_value_set direct);
enum_encoding_element_stateful enum_encoding_element_stateful_from_indirect(void);
bool enum_encoding_element_stateful_equals(enum_encoding_element_stateful const left,
                                           enum_encoding_element_stateful const right);

typedef struct enum_encoding_element_stateless
{
    ecmascript_value key;
} enum_encoding_element_stateless;

enum_encoding_element_stateless enum_encoding_element_stateless_create(ecmascript_value key);
bool enum_encoding_element_stateless_equals(enum_encoding_element_stateless const left,
                                            enum_encoding_element_stateless const right);

typedef struct enum_encoding_element
{
    union
    {
        enum_encoding_element_stateful stateful;
        enum_encoding_element_stateless stateless;
    };
    bool has_state;
} enum_encoding_element;

enum_encoding_element enum_encoding_element_from_stateful(enum_encoding_element_stateful stateful);
enum_encoding_element enum_encoding_element_from_stateless(enum_encoding_element_stateless stateless);
ecmascript_value_set enum_encoding_element_value_set(enum_encoding_element const from);
bool enum_encoding_element_equals(enum_encoding_element const left, enum_encoding_element const right);

typedef struct enum_encoding_strategy
{
    enum_encoding_element *elements;
    enum_element_id count;
} enum_encoding_strategy;

enum_encoding_strategy enum_encoding_strategy_create(enum_encoding_element *elements, enum_element_id count);
void enum_encoding_strategy_free(enum_encoding_strategy const freed);
ecmascript_value_set enum_encoding_strategy_value_set(enum_encoding_strategy const from);

typedef struct enum_encoding_strategy_cache
{
    enumeration const *all_enums;
    enum_encoding_strategy *entries;
    enum_id entry_count;
} enum_encoding_strategy_cache;

enum_encoding_strategy_cache enum_encoding_strategy_cache_create(enumeration const *all_enums,
                                                                 enum_id const number_of_enums);
void enum_encoding_strategy_cache_free(enum_encoding_strategy_cache const freed);
enum_encoding_strategy *enum_encoding_strategy_cache_require(enum_encoding_strategy_cache *cache,
                                                             enum_id const required);
