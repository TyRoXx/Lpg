#pragma once
#include <stdbool.h>
#include "lpg_stream_writer.h"

typedef struct compiler_flags
{
    bool compile_only;
} compiler_flags;

typedef struct compiler_arguments
{
    bool valid;
    char *file_name;
    compiler_flags flags;
} compiler_arguments;

bool run_cli(int const argc, LPG_NON_NULL(char **const argv), stream_writer const diagnostics);
