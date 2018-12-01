#include "lpg_enum_encoding.h"
#include "lpg_ecmascript_backend.h"

success_indicator enum_construct_stateful_begin(enum_encoding_element_stateful stateful, enum_element_id const which,
                                                stream_writer const ecmascript_output)
{
    switch (stateful.encoding)
    {
    case enum_encoding_element_stateful_encoding_direct:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "/*stateful enum direct state*/ "));
        break;

    case enum_encoding_element_stateful_encoding_indirect:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, which)));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        break;
    }
    return success_yes;
}

success_indicator enum_construct_stateful_end(enum_encoding_element_stateful stateful,
                                              stream_writer const ecmascript_output)
{
    switch (stateful.encoding)
    {
    case enum_encoding_element_stateful_encoding_direct:
        break;

    case enum_encoding_element_stateful_encoding_indirect:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "]"));
        break;
    }
    return success_yes;
}

static success_indicator generate_check_for_value_set(function_generation *const state,
                                                      ecmascript_value_set const check_for, register_id const instance,
                                                      stream_writer const ecmascript_output)
{
    ASSUME(!ecmascript_value_set_is_empty(check_for));
    if (ecmascript_value_set_equals(ecmascript_value_set_create_anything(), check_for))
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, "true"));
        return success_yes;
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "false"));
    if (check_for.string)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || (typeof ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ") === \"string\")"));
    }
    if (!ecmascript_integer_range_is_empty(check_for.integer))
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || (("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " >= "));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, check_for.integer.first)));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ") && ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " <= "));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, check_for.integer.after_last - 1)));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "))"));
    }
    if (check_for.bool_false)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " === false)"));
    }
    if (check_for.bool_true)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " === true)"));
    }
    if (check_for.array)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || (Array.isArray("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "))"));
    }
    if (check_for.object)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || (typeof ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ") === \"object\")"));
    }
    if (check_for.function)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || (typeof ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ") === \"function\")"));
    }
    if (check_for.null)
    {
        LPG_TO_DO();
    }
    if (check_for.undefined)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, " || ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " === undefined)"));
    }
    return success_yes;
}

success_indicator stateful_enum_case_check(function_generation *const state, register_id const instance,
                                           enum_encoding_element_stateful stateful, enum_element_id const check_for,
                                           stream_writer const ecmascript_output)
{
    switch (stateful.encoding)
    {
    case enum_encoding_element_stateful_encoding_direct:
        LPG_TRY(generate_check_for_value_set(state, stateful.direct, instance, ecmascript_output));
        break;

    case enum_encoding_element_stateful_encoding_indirect:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "(typeof "));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " !== \"number\") && ("));
        LPG_TRY(generate_register_read(state, instance, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "[0] === "));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, check_for)));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ")"));
        break;
    }
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

success_indicator stateful_enum_get_state(enum_encoding_strategy_cache *const strategy_cache, enum_id const enum_,
                                          enum_element_id const which, stream_writer const ecmascript_output)
{
    enum_encoding_strategy *const strategy = enum_encoding_strategy_cache_require(strategy_cache, enum_);
    ASSUME(strategy);
    enum_encoding_element const strategy_element = strategy->elements[which];
    ASSUME(strategy_element.has_state);
    switch (strategy_element.stateful.encoding)
    {
    case enum_encoding_element_stateful_encoding_direct:
        break;

    case enum_encoding_element_stateful_encoding_indirect:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "[1]"));
        break;
    }
    return success_yes;
}

success_indicator generate_stateless_enum_element(enum_element_id const which, stream_writer const ecmascript_output)
{
    return stream_writer_write_integer(ecmascript_output, integer_create(0, which));
}

success_indicator generate_enum_element(checked_function const *const current_function,
                                        enum_encoding_strategy_cache *const strategy_cache,
                                        enum_element_value const element, enumeration const *const enum_,
                                        checked_function const *all_functions, function_id const function_count,
                                        lpg_interface const *const all_interfaces, structure const *const all_structs,
                                        enumeration const *const all_enums, stream_writer const ecmascript_output)
{
    enum_encoding_strategy *const strategy =
        enum_encoding_strategy_cache_require(strategy_cache, (enum_id)(enum_ - all_enums));
    ASSUME(strategy);
    ASSUME(element.which < strategy->count);
    enum_encoding_element const strategy_element = strategy->elements[element.which];
    if (element.state)
    {
        LPG_TRY(enum_construct_stateful_begin(strategy_element.stateful, element.which, ecmascript_output));
        LPG_TRY(generate_value(current_function, strategy_cache, *element.state, element.state_type, all_functions,
                               function_count, all_interfaces, all_structs, all_enums, ecmascript_output));
        LPG_TRY(enum_construct_stateful_end(strategy_element.stateful, ecmascript_output));
        return success_yes;
    }
    return generate_ecmascript_value(strategy_element.stateless.key, ecmascript_output);
}
