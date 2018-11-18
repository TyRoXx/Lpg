#pragma once
#include "lpg_structure_member.h"

external_function_result not_impl(function_call_arguments const arguments, struct value const *const captures,
                                  void *environment);

external_function_result concat_impl(function_call_arguments const arguments, struct value const *const captures,
                                     void *environment);

external_function_result string_equals_impl(function_call_arguments const arguments, struct value const *const captures,
                                            void *environment);

external_function_result int_impl(function_call_arguments const arguments, struct value const *const captures,
                                  void *environment);

external_function_result fail_impl(function_call_arguments const arguments, struct value const *const captures,
                                   void *environment);

external_function_result integer_equals_impl(function_call_arguments const arguments,
                                             struct value const *const captures, void *environment);

external_function_result integer_less_impl(function_call_arguments const arguments, struct value const *const captures,
                                           void *environment);

external_function_result integer_to_string_impl(function_call_arguments const arguments,
                                                struct value const *const captures, void *environment);

external_function_result subtract_impl(function_call_arguments const arguments, struct value const *const captures,
                                       void *environment);

external_function_result add_impl(function_call_arguments const arguments, struct value const *const captures,
                                  void *environment);

external_function_result side_effect_impl(function_call_arguments const arguments, struct value const *const captures,
                                          void *environment);

typedef struct standard_library_stable
{
    function_pointer assert_;
    function_pointer not_;
    function_pointer concat;
    function_pointer string_equals;
    function_pointer int_;
    function_pointer integer_equals;
    function_pointer integer_less;
    function_pointer integer_to_string;
    function_pointer side_effect;
    function_pointer type_equals;
    function_pointer fail;
    function_pointer subtract;
    function_pointer add;
} standard_library_stable;

enum
{
    standard_library_enum_boolean = 0,
    standard_library_enum_subtract_result = 1,
    standard_library_enum_add_result = 2,
    standard_library_element_count = 17
};

typedef struct standard_library_description
{
    structure globals;
    standard_library_stable *stable;
} standard_library_description;

standard_library_description describe_standard_library(void);
void standard_library_description_free(LPG_NON_NULL(standard_library_description const *freed));
