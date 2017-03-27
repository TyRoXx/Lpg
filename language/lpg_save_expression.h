#pragma once
#include "lpg_stream_writer.h"
#include "lpg_expression.h"

success_indicator save_expression(stream_writer const to,
                                  expression const *value, size_t indentation);
