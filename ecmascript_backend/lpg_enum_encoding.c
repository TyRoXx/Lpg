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
