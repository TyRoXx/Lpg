#pragma once

#include "lpg_enum_element_id.h"
#include "lpg_enum_encoding_strategy.h"
#include "lpg_function_generation.h"
#include "lpg_register.h"
#include "lpg_stream_writer.h"

success_indicator enum_construct_stateful_begin(enum_element_id const which, stream_writer const ecmascript_output);
success_indicator enum_construct_stateful_end(stream_writer const ecmascript_output);
success_indicator stateful_enum_case_check(function_generation *const state, register_id const instance,
                                           enum_element_id const check_for, stream_writer const ecmascript_output);
success_indicator generate_stateless_enum_case_check(function_generation *const state, register_id const input,
                                                     register_id const case_key, stream_writer const ecmascript_output);
success_indicator stateful_enum_get_state(stream_writer const ecmascript_output);
success_indicator generate_stateless_enum_element(enum_element_id const which, stream_writer const ecmascript_output);
success_indicator generate_enum_element(enum_encoding_strategy_cache *const strategy_cache,
                                        enum_element_value const element, enumeration const *const enum_,
                                        checked_function const *all_functions, function_id const function_count,
                                        lpg_interface const *const all_interfaces, structure const *const all_structs,
                                        enumeration const *const all_enums, stream_writer const ecmascript_output);
