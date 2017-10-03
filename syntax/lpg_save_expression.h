#pragma once
#include "lpg_stream_writer.h"
#include "lpg_expression.h"

typedef struct whitespace_state
{
    size_t indentation_depth;
    bool pending_space;
} whitespace_state;

success_indicator save_expression(stream_writer const to, LPG_NON_NULL(expression const *value),
                                  whitespace_state whitespace);
