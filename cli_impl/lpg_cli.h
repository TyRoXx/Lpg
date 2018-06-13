#pragma once
#include <stdbool.h>
#include "lpg_stream_writer.h"

typedef enum compiler_command
{
    compiler_command_run = 1,
    compiler_command_compile,
    compiler_command_format
} compiler_command;

typedef struct compiler_arguments
{
    bool valid;
    compiler_command command;
    char *file_name;
} compiler_arguments;

bool run_cli(int const argc, LPG_NON_NULL(char **const argv), stream_writer const diagnostics,
             unicode_view const module_directory);
