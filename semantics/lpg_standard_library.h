#pragma once
#include "lpg_structure_member.h"

value not_impl(function_call_arguments const arguments, struct value const *const captures, void *environment);

value concat_impl(function_call_arguments const arguments, struct value const *const captures, void *environment);

value and_impl(function_call_arguments const arguments, struct value const *const captures, void *environment);

value or_impl(function_call_arguments const arguments, struct value const *const captures, void *environment);

value string_equals_impl(function_call_arguments const arguments, struct value const *const captures,
                         void *environment);

value int_impl(function_call_arguments const arguments, struct value const *const captures, void *environment);

value integer_equals_impl(function_call_arguments const arguments, struct value const *const captures,
                          void *environment);

value integer_less_impl(function_call_arguments const arguments, struct value const *const captures, void *environment);

value integer_to_string_impl(function_call_arguments const arguments, struct value const *const captures,
                             void *environment);

value side_effect_impl(function_call_arguments const arguments, struct value const *const captures, void *environment);

typedef struct standard_library_stable
{
    function_pointer assert_;
    function_pointer and_;
    function_pointer or_;
    function_pointer not_;
    function_pointer concat;
    function_pointer string_equals;
    function_pointer int_;
    function_pointer integer_equals;
    function_pointer integer_less;
    function_pointer integer_to_string;
    function_pointer side_effect;
} standard_library_stable;

enum
{
    standard_library_element_count = 19
};

typedef struct standard_library_description
{
    structure globals;
    standard_library_stable *stable;
} standard_library_description;

standard_library_description describe_standard_library(void);
void standard_library_description_free(LPG_NON_NULL(standard_library_description const *value));
