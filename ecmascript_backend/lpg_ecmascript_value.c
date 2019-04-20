#include "lpg_ecmascript_value.h"

ecmascript_value ecmascript_value_create_integer(uint64_t value)
{
    ecmascript_value result;
    result.type = ecmascript_value_integer;
    result.integer = value;
    return result;
}

ecmascript_value ecmascript_value_create_boolean(bool value)
{
    ecmascript_value result;
    result.type = ecmascript_value_boolean;
    result.boolean = value;
    return result;
}

ecmascript_value ecmascript_value_create_undefined(void)
{
    ecmascript_value result;
    result.type = ecmascript_value_undefined;
    return result;
}

success_indicator generate_ecmascript_value(ecmascript_value const value, stream_writer const ecmascript_output)
{
    switch (value.type)
    {
    case ecmascript_value_boolean:
        if (value.boolean)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, "true"));
        }
        else
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, "false"));
        }
        break;

    case ecmascript_value_integer:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "/*enum*/ "));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, value.integer)));
        break;

    case ecmascript_value_null:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "null"));
        break;

    case ecmascript_value_undefined:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "undefined"));
        break;
    }
    return success_yes;
}

bool ecmascript_value_equals(ecmascript_value const left, ecmascript_value const right)
{
    if (left.type != right.type)
    {
        return false;
    }
    switch (left.type)
    {
    case ecmascript_value_integer:
        return (left.integer == right.integer);

    case ecmascript_value_boolean:
        return (left.boolean == right.boolean);

    case ecmascript_value_null:
    case ecmascript_value_undefined:
        return true;
    }
    LPG_UNREACHABLE();
}
