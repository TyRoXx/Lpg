#pragma once
#include "lpg_structure_member.h"

value not_impl(value const *const inferred, value const *arguments,
               garbage_collector *const gc, void *environment);

value concat_impl(value const *const inferred, value const *const arguments,
                  garbage_collector *const gc, void *environment);

value and_impl(value const *const inferred, value const *arguments,
               garbage_collector *const gc, void *environment);

value or_impl(value const *const inferred, value const *arguments,
              garbage_collector *const gc, void *environment);

value string_equals_impl(value const *const inferred,
                         value const *const arguments,
                         garbage_collector *const gc, void *environment);

typedef struct standard_library_stable
{
    enumeration boolean;
    function_pointer print;
    function_pointer assert_;
    function_pointer and_;
    function_pointer or_;
    function_pointer not_;
    function_pointer concat;
    function_pointer string_equals;
    function_pointer read;
    function_pointer int_;
    function_pointer integer_equals;
} standard_library_stable;

typedef struct standard_library_description
{
    structure globals;
    standard_library_stable *stable;
} standard_library_description;

standard_library_description describe_standard_library(void);
void standard_library_description_free(
    LPG_NON_NULL(standard_library_description const *value));
