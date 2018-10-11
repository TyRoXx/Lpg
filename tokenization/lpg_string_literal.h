#pragma once

#include <lpg_stream_writer.h>
#include <lpg_unicode_view.h>
#include <stdbool.h>

typedef struct decode_string_literal_result
{
    bool is_valid;
    size_t length;
} decode_string_literal_result;

decode_string_literal_result decode_string_literal(unicode_view source, stream_writer decoded);
decode_string_literal_result decode_string_literal_result_create(bool is_valid, size_t length);
