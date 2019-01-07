#pragma once
#include "lpg_checked_program.h"
#include "lpg_enum_encoding_strategy.h"
#include "lpg_function_generation.h"
#include "lpg_stream_writer.h"

success_indicator generate_value(checked_function const *const current_function,
                                 enum_encoding_strategy_cache *const strategy_cache, value const generated,
                                 type const type_of, checked_function const *all_functions,
                                 function_id const function_count, lpg_interface const *const all_interfaces,
                                 structure const *const all_structs, enumeration const *const all_enums,
                                 stream_writer const ecmascript_output);

success_indicator generate_register_read(function_generation *const state, register_id const id,
                                         stream_writer const ecmascript_output);

lpg_interface make_array_interface(method_description array_methods[6]);
success_indicator generate_ecmascript(checked_program const program, enum_encoding_strategy_cache *strategy_cache,
                                      unicode_view const builtins, memory_writer *const ecmascript_output);

success_indicator generate_host_class(enum_encoding_strategy_cache *const strategy_cache,
                                      lpg_interface const *const host, lpg_interface const *const all_interfaces,
                                      stream_writer const ecmascript_output);
lpg_interface const *get_host_interface(checked_program const program);

unicode_string load_ecmascript_builtins(void);
