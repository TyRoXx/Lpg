#pragma once

#include "lpg_expression.h"
#include "lpg_type.h"
#include "lpg_value.h"

typedef struct generic_closure
{
    unicode_string name;
    type what;
    value content;
} generic_closure;

generic_closure generic_closure_create(unicode_string name, type what, value content);
void generic_closure_free(generic_closure const freed);

typedef struct generic_closures
{
    generic_closure *elements;
    size_t count;
} generic_closures;

void generic_closures_free(generic_closures const freed);

typedef struct generic_impl
{
    impl_expression tree;
    generic_closures closures;
    type self;
} generic_impl;

generic_impl generic_impl_create(impl_expression tree, generic_closures closures, type self);
void generic_impl_free(generic_impl const freed);
