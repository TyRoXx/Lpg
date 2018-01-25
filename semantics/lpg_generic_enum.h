#pragma once

#include "lpg_expression.h"
#include "lpg_value.h"

typedef struct generic_enum_closure
{
    unicode_string name;
    type what;
    value content;
} generic_enum_closure;

generic_enum_closure generic_enum_closure_create(unicode_string name, type what, value content);
void generic_enum_closure_free(generic_enum_closure const freed);

typedef struct generic_enum
{
    enum_expression tree;
    generic_enum_closure *closures;
    size_t closure_count;
} generic_enum;

generic_enum generic_enum_create(enum_expression tree, generic_enum_closure *closures, size_t closure_count);
void generic_enum_free(generic_enum const freed);
