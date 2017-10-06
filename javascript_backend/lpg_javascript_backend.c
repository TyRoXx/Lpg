#include "lpg_javascript_backend.h"

success_indicator generate_javascript(checked_program const program, stream_writer const javascript_output)
{
    (void)program;
    return stream_writer_write_string(javascript_output, "print(\"test\");\n");
}
