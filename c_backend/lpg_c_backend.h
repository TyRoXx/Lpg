#pragma once
#include "lpg_stream_writer.h"
#include "lpg_checked_program.h"

success_indicator generate_c(checked_program const program,
                             stream_writer const c_output);
