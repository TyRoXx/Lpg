#pragma once
#include "lpg_checked_program.h"
#include "lpg_stream_writer.h"

success_indicator generate_ecmascript(checked_program const program, stream_writer const ecmascript_output);

success_indicator generate_host_class(stream_writer const destination);
