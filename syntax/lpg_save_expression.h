#pragma once
#include "lpg_expression.h"
#include "lpg_stream_writer.h"

typedef struct whitespace_state
{
    size_t indentation_depth;
    bool pending_space;
} whitespace_state;

success_indicator save_sequence(stream_writer const to, sequence const value, whitespace_state whitespace);
success_indicator save_expression(stream_writer const to, LPG_NON_NULL(expression const *value),
                                  whitespace_state whitespace);
