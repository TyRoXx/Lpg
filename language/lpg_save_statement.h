#pragma once
#include "lpg_stream_writer.h"
#include "lpg_statement.h"

success_indicator save_statement(stream_writer to, statement const *value);
