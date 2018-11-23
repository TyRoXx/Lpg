#include "lpg_enum_encoding.h"
#include "lpg_ecmascript_backend.h"

success_indicator enum_construct_stateful_begin(enum_element_id const which, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, which)));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
    return success_yes;
}

success_indicator enum_construct_stateful_end(stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "]"));
    return success_yes;
}

success_indicator stateful_enum_case_check(function_generation *const state, register_id const instance,
                                           enum_element_id const check_for, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "(typeof "));
    LPG_TRY(generate_register_read(state, instance, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " !== \"number\") && ("));
    LPG_TRY(generate_register_read(state, instance, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "[0] === "));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, check_for)));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ")"));
    return success_yes;
}

success_indicator generate_stateless_enum_case_check(function_generation *const state, register_id const input,
                                                     register_id const case_key, stream_writer const ecmascript_output)
{
    LPG_TRY(generate_register_read(state, input, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " === "));
    LPG_TRY(generate_register_read(state, case_key, ecmascript_output));
    return success_yes;
}

success_indicator stateful_enum_get_state(stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "[1]"));
    return success_yes;
}

success_indicator generate_stateless_enum_element(enum_element_id const which, stream_writer const ecmascript_output)
{
    return stream_writer_write_integer(ecmascript_output, integer_create(0, which));
}

success_indicator generate_enum_element(enum_encoding_strategy_cache *const strategy_cache,
                                        enum_element_value const element, enumeration const *const enum_,
                                        checked_function const *all_functions, function_id const function_count,
                                        lpg_interface const *const all_interfaces, structure const *const all_structs,
                                        enumeration const *const all_enums, stream_writer const ecmascript_output)
{
    enum_encoding_strategy *const strategy =
        enum_encoding_strategy_cache_require(strategy_cache, (enum_id)(enum_ - all_enums));
    ASSUME(strategy);
    enum_encoding_element const strategy_element = strategy->elements[element.which];
    if (element.state)
    {
        switch (strategy_element.stateful.encoding)
        {
        case enum_encoding_element_stateful_encoding_direct:
            LPG_TO_DO();

        case enum_encoding_element_stateful_encoding_indirect:
            LPG_TRY(enum_construct_stateful_begin(element.which, ecmascript_output));
            LPG_TRY(generate_value(strategy_cache, *element.state, element.state_type, all_functions, function_count,
                                   all_interfaces, all_structs, all_enums, ecmascript_output));
            LPG_TRY(enum_construct_stateful_end(ecmascript_output));
            break;
        }
        return success_yes;
    }
    return generate_ecmascript_value(strategy_element.stateless.key, ecmascript_output);
}
