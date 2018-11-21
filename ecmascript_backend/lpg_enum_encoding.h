#pragma once

#include "lpg_enum_element_id.h"
#include "lpg_stream_writer.h"

success_indicator enum_construct_stateful_begin(enum_element_id const which, stream_writer const ecmascript_output);
success_indicator enum_construct_stateful_end(stream_writer const ecmascript_output);
