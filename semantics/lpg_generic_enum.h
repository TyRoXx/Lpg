#pragma once

#include "lpg_expression.h"
#include "lpg_value.h"

typedef struct generic_closure
{
    unicode_string name;
    type what;
    value content;
} generic_closure;

generic_closure generic_closure_create(unicode_string name, type what, value content);
void generic_closure_free(generic_closure const freed);

typedef struct generic_enum
{
    enum_expression tree;
    generic_closure *closures;
    size_t closure_count;
} generic_enum;

generic_enum generic_enum_create(enum_expression tree, generic_closure *closures, size_t closure_count);
void generic_enum_free(generic_enum const freed);
