#pragma once
#include "lpg_stream_writer.h"
#include "lpg_checked_program.h"
#include "lpg_type.h"

success_indicator generate_c(checked_program const program, garbage_collector *const additional_memory,
                             stream_writer const c_output);
