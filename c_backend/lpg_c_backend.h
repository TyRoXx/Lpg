#pragma once
#include "lpg_stream_writer.h"
#include "lpg_checked_program.h"

success_indicator generate_c(checked_program const program,
                             stream_writer const c_output);

#define LPG_C_STRING_REF                                                       \
    "#include <stdlib.h>\n"                                                    \
    "typedef struct string_ref\n"                                              \
    "{\n"                                                                      \
    "    char *data;\n"                                                        \
    "    size_t length;\n"                                                     \
    "} string_ref;\n"                                                          \
    "static void string_ref_free(string_ref const *s)\n"                       \
    "{\n"                                                                      \
    "    free(s->data);\n"                                                     \
    "}\n"

#define LPG_C_READ                                                             \
    "static string_ref read_impl(void)\n"                                      \
    "{\n"                                                                      \
    "    /*TODO*/\n"                                                           \
    "}\n"

#define LPG_C_STDIO "#include <stdio.h>\n"
