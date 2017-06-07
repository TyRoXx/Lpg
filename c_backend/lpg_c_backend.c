#include "lpg_c_backend.h"

success_indicator generate_c(checked_program const program,
                             stream_writer const c_output)
{
    (void)program;
    LPG_TRY(stream_writer_write_string(c_output, "#include <stdio.h>\n"));
    LPG_TRY(stream_writer_write_string(c_output, "int main(void)\n"));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));
    LPG_TRY(stream_writer_write_string(c_output, "    return 0;\n"));
    LPG_TRY(stream_writer_write_string(c_output, "}\n"));
    return success;
}
