#include "lpg_enum_encoding.h"

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
