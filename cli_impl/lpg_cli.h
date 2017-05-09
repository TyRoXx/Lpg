#pragma once
#include <stdbool.h>
#include "lpg_stream_writer.h"

bool run_cli(int const argc, char **const argv, stream_writer const diagnostics,
             stream_writer print_destination);
