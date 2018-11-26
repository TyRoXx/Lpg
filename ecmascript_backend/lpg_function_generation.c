#include "lpg_function_generation.h"

register_info register_info_create(register_type kind, type type_of, optional_value known_value,
                                   size_t declaration_begin, size_t declaration_end, bool is_ever_used)
{
    register_info const result = {kind, type_of, known_value, declaration_begin, declaration_end, is_ever_used};
    return result;
}

function_generation function_generation_create(register_info *registers, checked_function const *all_functions,
                                               function_id const function_count, lpg_interface const *all_interfaces,
                                               enumeration const *all_enums, structure const *all_structs,
                                               checked_function const *current_function,
                                               enum_encoding_strategy_cache *strategy_cache,
                                               size_t *const current_output_position)
{
    function_generation const result = {registers,        all_functions,  function_count,
                                        all_interfaces,   all_enums,      all_structs,
                                        current_function, strategy_cache, current_output_position};
    return result;
}
