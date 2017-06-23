#pragma once
#include "lpg_stream_writer.h"
#include "lpg_checked_program.h"

success_indicator generate_c(checked_program const program,
                             stream_writer const c_output);

#define LPG_C_STDLIB "#include <stdlib.h>\n"

#define LPG_C_STDBOOL "#include <stdbool.h>\n"

#define LPG_C_STRING "#include <string.h>\n"

#define LPG_C_STRING_REF                                                       \
    "typedef struct string_ref\n"                                              \
    "{\n"                                                                      \
    "    char *data;\n"                                                        \
    "    size_t length;\n"                                                     \
    "} string_ref;\n"                                                          \
    "static string_ref string_ref_create(char *data, size_t length)\n"         \
    "{\n"                                                                      \
    "    string_ref result = {data, length};\n"                                \
    "    return result;\n"                                                     \
    "}\n"                                                                      \
    "static void string_ref_free(string_ref const *s)\n"                       \
    "{\n"                                                                      \
    "    free(s->data);\n"                                                     \
    "}\n"                                                                      \
    "static bool string_ref_equals(string_ref const left, string_ref const "   \
    "right)\n"                                                                 \
    "{\n"                                                                      \
    "    if (left.length != right.length)\n"                                   \
    "    {\n"                                                                  \
    "        return false;\n"                                                  \
    "    }\n"                                                                  \
    "    return !memcmp(left.data, right.data, left.length);\n"                \
    "}\n"

#define LPG_C_READ                                                             \
    "static string_ref read_impl(void)\n"                                      \
    "{\n"                                                                      \
    "    /*TODO*/\n"                                                           \
    "}\n"

#define LPG_C_STDIO "#include <stdio.h>\n"

#define LPG_C_ASSERT                                                           \
    "static void assert_impl(bool const condition)\n"                          \
    "{\n"                                                                      \
    "    if (!condition)\n"                                                    \
    "    {\n"                                                                  \
    "        abort();\n"                                                       \
    "    }\n"                                                                  \
    "}\n"
