#pragma once
#include "lpg_checked_program.h"
#include "lpg_function_generation.h"
#include "lpg_stream_writer.h"

success_indicator generate_value(value const generated, type const type_of, checked_function const *all_functions,
                                 function_id const function_count, lpg_interface const *const all_interfaces,
                                 structure const *const all_structs, enumeration const *const all_enums,
                                 stream_writer const ecmascript_output);

success_indicator generate_register_read(function_generation *const state, register_id const id,
                                         stream_writer const ecmascript_output);

lpg_interface make_array_interface(method_description array_methods[6]);
success_indicator generate_ecmascript(checked_program const program, stream_writer const ecmascript_output);

success_indicator generate_host_class(lpg_interface const *const host, stream_writer const ecmascript_output);
lpg_interface const *get_host_interface(checked_program const program);
