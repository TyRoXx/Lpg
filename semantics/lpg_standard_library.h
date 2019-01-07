#pragma once
#include "lpg_interpret.h"
#include "lpg_structure_member.h"

external_function_result not_impl(value const *const captures, void *environment, optional_value const self,
                                  value *const arguments, interpreter *const context);

external_function_result concat_impl(value const *const captures, void *environment, optional_value const self,
                                     value *const arguments, interpreter *const context);

external_function_result string_equals_impl(value const *const captures, void *environment, optional_value const self,
                                            value *const arguments, interpreter *const context);

external_function_result int_impl(value const *const captures, void *environment, optional_value const self,
                                  value *const arguments, interpreter *const context);

external_function_result fail_impl(value const *const captures, void *environment, optional_value const self,
                                   value *const arguments, interpreter *const context);

external_function_result integer_equals_impl(value const *const captures, void *environment, optional_value const self,
                                             value *const arguments, interpreter *const context);

external_function_result integer_less_impl(value const *const captures, void *environment, optional_value const self,
                                           value *const arguments, interpreter *const context);

external_function_result integer_to_string_impl(value const *const captures, void *environment,
                                                optional_value const self, value *const arguments,
                                                interpreter *const context);

external_function_result subtract_impl(value const *const captures, void *environment, optional_value const self,
                                       value *const arguments, interpreter *const context);

external_function_result add_impl(value const *const captures, void *environment, optional_value const self,
                                  value *const arguments, interpreter *const context);

external_function_result add_u32_impl(value const *const captures, void *environment, optional_value const self,
                                      value *const arguments, interpreter *const context);

external_function_result add_u64_impl(value const *const captures, void *environment, optional_value const self,
                                      value *const arguments, interpreter *const context);

external_function_result side_effect_impl(value const *const captures, void *environment, optional_value const self,
                                          value *const arguments, interpreter *const context);

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
    function_pointer add_u32;
    function_pointer add_u64;
    function_pointer and_u64;
} standard_library_stable;

enum
{
    standard_library_enum_boolean = 0,
    standard_library_enum_subtract_result = 1,
    standard_library_enum_add_result = 2,
    standard_library_enum_add_u32_result = 3,
    standard_library_enum_add_u64_result = 4,
    standard_library_element_count = 22
};

typedef struct standard_library_description
{
    structure globals;
    standard_library_stable *stable;
} standard_library_description;

standard_library_description describe_standard_library(void);
void standard_library_description_free(LPG_NON_NULL(standard_library_description const *freed));
