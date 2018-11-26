#pragma once

#include "lpg_stream_writer.h"

typedef enum ecmascript_value_type {
    ecmascript_value_integer,
    ecmascript_value_undefined,
    ecmascript_value_null,
    ecmascript_value_boolean
} ecmascript_value_type;

typedef struct ecmascript_value
{
    ecmascript_value_type type;
    union
    {
        uint64_t integer;
        bool boolean;
    };
} ecmascript_value;

ecmascript_value ecmascript_value_create_integer(uint64_t value);
ecmascript_value ecmascript_value_create_boolean(bool value);
ecmascript_value ecmascript_value_create_undefined(void);
success_indicator generate_ecmascript_value(ecmascript_value const value, stream_writer const ecmascript_output);
