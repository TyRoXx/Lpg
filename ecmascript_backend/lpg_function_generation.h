#pragma once

#include "lpg_enum_encoding_strategy.h"
#include "lpg_value.h"

typedef enum register_type {
    register_type_none = 1,
    register_type_variable,
    register_type_global,
    register_type_captures
} register_type;

typedef struct register_info
{
    register_type kind;
    type type_of;
    optional_value known_value;
    // if variable:
    size_t declaration_begin;
    size_t declaration_end;
    // if variable:
    bool is_ever_used;
} register_info;

register_info register_info_create(register_type kind, type type_of, optional_value known_value,
                                   size_t declaration_begin, size_t declaration_end, bool is_ever_used);

typedef struct function_generation
{
    register_info *registers;
    checked_function const *all_functions;
    function_id function_count;
    lpg_interface const *all_interfaces;
    enumeration const *all_enums;
    structure const *all_structs;
    checked_function const *current_function;
    enum_encoding_strategy_cache *strategy_cache;
    size_t *const current_output_position;
} function_generation;

function_generation function_generation_create(register_info *registers, checked_function const *all_functions,
                                               function_id const function_count, lpg_interface const *all_interfaces,
                                               enumeration const *all_enums, structure const *all_structs,
                                               checked_function const *current_function,
                                               enum_encoding_strategy_cache *strategy_cache,
                                               size_t *const current_output_position);
