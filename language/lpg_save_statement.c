#include "lpg_save_statement.h"
#include "lpg_save_expression.h"
#include "lpg_assert.h"

success_indicator save_statement(stream_writer to, statement const *value)
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
        LPG_TRY(stream_writer_write_string(to, "loop\n    "));
        return save_statement(to, value->loop_body);

    case statement_break:
        return stream_writer_write_string(to, "break\n");
    }
    UNREACHABLE();
}
