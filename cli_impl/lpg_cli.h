#pragma once
#include <stdbool.h>
#include "lpg_stream_writer.h"

bool run_cli(int const argc, LPG_NON_NULL(char **const argv),
             stream_writer const diagnostics, stream_writer print_destination);
