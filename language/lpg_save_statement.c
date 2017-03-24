#include "lpg_save_statement.h"
#include "lpg_save_expression.h"
#include "lpg_assert.h"
#include "lpg_for.h"

success_indicator save_statement(stream_writer to, statement const *value,
                                 size_t indentation)
{
    switch (value->type)
    {
    case statement_assign:
        LPG_TRY(save_expression(to, value->assign.left));
        LPG_TRY(stream_writer_write_string(to, " = "));
        LPG_TRY(save_expression(to, value->assign.right));
        return stream_writer_write_string(to, "\n");

    case statement_return:
        LPG_TRY(stream_writer_write_string(to, "return "));
        LPG_TRY(save_expression(to, value->return_));
        return stream_writer_write_string(to, "\n");

    case statement_expression:
        LPG_TRY(save_expression(to, value->expression));
        return stream_writer_write_string(to, "\n");

    case statement_loop:
        LPG_TRY(stream_writer_write_string(to, "loop\n"));
        LPG_FOR(size_t, i, (indentation + 1))
        {
            LPG_TRY(stream_writer_write_string(to, "    "));
        }
        return save_statement(to, value->loop_body, indentation + 1);

    case statement_break:
        return stream_writer_write_string(to, "break\n");

    case statement_sequence:
        LPG_FOR(size_t, i, value->sequence.length)
        {
            if (i > 0)
            {
                LPG_FOR(size_t, j, indentation)
                {
                    LPG_TRY(stream_writer_write_string(to, "    "));
                }
            }
            LPG_TRY(
                save_statement(to, value->sequence.elements + i, indentation));
        }
        return success;
    }
    UNREACHABLE();
}
