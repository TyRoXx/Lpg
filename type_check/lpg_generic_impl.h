#pragma once

#include "lpg_expression.h"
#include "lpg_source_file.h"
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

generic_closures generic_closures_create(generic_closure *elements, size_t count);
void generic_closures_free(generic_closures const freed);

typedef struct generic_impl_self
{
    bool is_regular;
    type regular;
    generic_instantiation_expression generic;
} generic_impl_self;

generic_impl_self generic_impl_self_create_regular(type regular);
generic_impl_self generic_impl_self_create_generic(generic_instantiation_expression generic);
void generic_impl_self_free(generic_impl_self const freed);

typedef struct generic_impl
{
    impl_expression tree;
    generic_closures closures;
    generic_impl_self self;
    source_file_owning const *source;
    unicode_string current_import_directory;
} generic_impl;

generic_impl generic_impl_create(impl_expression tree, generic_closures closures, generic_impl_self self,
                                 source_file_owning const *source, unicode_string current_import_directory);
void generic_impl_free(generic_impl const freed);
